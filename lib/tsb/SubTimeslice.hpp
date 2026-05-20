/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Jan de Cuveland, Dirk Hutter */
#pragma once

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_serialize.hpp>
#include <cstddef>
#include <format>
#include <functional>
#include <log.hpp>
#include <span>
#include <string>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <vector>

// Strongly typed timeslice (or subtimeslice) identifier

struct TsId {
  TsId(uint64_t v) : value(v) {}
  TsId() : value(0) {}

  bool operator==(const TsId& other) const { return value == other.value; }
  operator uint64_t() const { return value; }

  uint64_t value;

  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & value;
  }
};

template <> struct std::hash<TsId> {
  std::size_t operator()(const TsId& id) const noexcept {
    return std::hash<uint64_t>{}(id.value);
  }
};

template <> struct std::formatter<TsId> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const TsId& id, format_context& ctx) const {
    std::string id_str = std::to_string(id.value);
    if (id_str.length() > 6) {
      return std::format_to(ctx.out(), "ts..{}",
                            id_str.substr(id_str.length() - 3));
    }
    return std::format_to(ctx.out(), "ts{}", id.value);
  }
};

// Flags

enum class TsComponentFlag : uint32_t {
  None = 0,

  // Microslices are missing in this component
  MissingMicroslices = 1 << 0,

  // One or more microslices in this component have their "OverflowFlim" flag
  // set
  OverflowFlim = 1 << 1
};

enum class TsFlag : uint32_t {
  None = 0,

  // One or more components are missing microslices
  // (i.e., at least one component has
  // StComponentFlag::IsMissingMicroslices set)
  MissingMicroslices = 1 << 0,

  // One or more components in this subtimeslice have
  // StComponentFlag::OverflowFlim set, i.e., at least one microslice has its
  // "OverflowFlim" flag set
  OverflowFlim = 1 << 1,

  // Subtimeslice is incomplete due to missing components
  MissingComponents = 1 << 2,

  // Timeslice is incomplete due to missing subtimeslices
  MissingSubtimeslices = 1 << 3,
};

// 1: sender only
//
// Internal structures for transferring subtimeslice memory handles to the
// StSender

struct StComponentHandle {
  std::vector<ucp_dt_iov> ms_data;
  std::size_t num_microslices = 0;
  uint32_t flags = 0;

  void set_flag(TsComponentFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(TsComponentFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(TsComponentFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// The number of microslice data (descriptors + contents) bytes
  [[nodiscard]] uint64_t ms_data_size() const {
    uint64_t size = 0;
    for (const auto& sg : ms_data) {
      size += sg.length;
    }
    return size;
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const StComponentHandle& i) {
    return os << "StComponentHandle(num_microslices=" << i.num_microslices
              << ", flags=" << i.flags << ")";
  }
};

struct StHandle {
  uint64_t start_time_ns = 0;
  uint64_t duration_ns = 0;
  uint32_t flags = 0;
  std::vector<StComponentHandle> components;

  void set_flag(TsFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(TsFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(TsFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// Dump contents (for debugging)
  friend std::ostream& operator<<(std::ostream& os, const StHandle& i) {
    return os << "StHandle(start_time_ns=" << i.start_time_ns
              << ", duration_ns=" << i.duration_ns << ", flags=" << i.flags
              << ", components=...)";
  }
};

// Sender and builder information for registration with the scheduler

struct SenderInfo {
  std::string hostname;
  int pid = 0;
  std::string address;
  uint16_t port = 0;

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & hostname;
    ar & pid;
    ar & address;
    ar & port;
  }

  [[nodiscard]] std::string id() const {
    return std::format("{}#{}", hostname, pid);
  }
  [[nodiscard]] std::string advertise_id() const {
    return std::format("{}:{}", address, port);
  }
};

struct BuilderInfo {
  std::string hostname;
  int pid = 0;

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & hostname;
    ar & pid;
  }

  [[nodiscard]] std::string id() const {
    return std::format("{}#{}", hostname, pid);
  }
};

template <> struct std::formatter<SenderInfo> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const SenderInfo& si, format_context& ctx) const {
    return std::format_to(ctx.out(), "{}#{}/{}:{}", si.hostname, si.pid,
                          si.address, si.port);
  }
};

template <> struct std::formatter<BuilderInfo> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const BuilderInfo& bi, format_context& ctx) const {
    return std::format_to(ctx.out(), "{}#{}", bi.hostname, bi.pid);
  }
};

// 2: sender -> builder and sender -> scheduler
//
// Descriptors for transferring subtimeslice data from the sender to the builder
// (and to the scheduler, for statistics)

struct StComponentDescriptor {
  /// A data descriptor pointing to the microslice descriptor data blocks,
  /// followed by the microslice content data blocks
  std::ptrdiff_t ms_data_offset = 0;
  std::size_t ms_data_size = 0;

  /// The number of microslices in this component
  std::size_t num_microslices = 0;

  /// Flags
  uint32_t flags = 0;

  // Explicit padding so the on-wire layout is unambiguous and matches the
  // C++ struct size (trailing 4-byte alignment hole made explicit).
  uint32_t reserved_ = 0;

  void set_flag(TsComponentFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(TsComponentFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(TsComponentFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }
};
static_assert(std::is_trivially_copyable_v<StComponentDescriptor>);
static_assert(sizeof(StComponentDescriptor) == 32);

struct StDescriptor {
  /// The start time of the subtimeslice in nanoseconds, should be divisible by
  /// the duration (duration_ns)
  uint64_t start_time_ns = 0;

  /// The duration of the subtimeslice in nanoseconds
  uint64_t duration_ns = 0;

  /// Flags
  uint32_t flags = 0;

  /// The subtimeslice component descriptors
  std::vector<StComponentDescriptor> components;

  void set_flag(TsFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(TsFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(TsFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// The total size (in bytes) of the subtimeslice (i.e., microslice
  /// descriptors + content of all contained components)
  [[nodiscard]] uint64_t ms_data_size() const {
    uint64_t total_ms_data_size = 0;
    for (const auto& component : components) {
      total_ms_data_size += component.ms_data_size;
    }
    return total_ms_data_size;
  }
};

// 3: scheduler -> builder
//
// Descriptor for transferring timeslice metadata from the scheduler to the
// builder

struct StCollection {
  TsId id = 0;

  std::vector<std::string> sender_ids; // IDs of the senders
  std::vector<uint64_t> ms_data_sizes; // Sizes of the content data

  /// Merged subtimeslice descriptor (aggregated by the scheduler from all
  /// announced contributions). Component offsets are absolute, i.e. already
  /// shifted by the per-sender offsets derived from ms_data_sizes.
  StDescriptor merged_descriptor;

  /// The total size (in bytes) of the collection (i.e., microslice
  /// descriptors + content of all contained components)
  [[nodiscard]] uint64_t ms_data_size() const {
    uint64_t total_ms_data_size = 0;
    for (const auto& size : ms_data_sizes) {
      total_ms_data_size += size;
    }
    return total_ms_data_size;
  }
};

// Specialize std::formatter for std::vector to simplify debugging
template <typename T> struct std::formatter<std::vector<T>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const std::vector<T>& vec, format_context& ctx) const {
    auto out = ctx.out();
    *out++ = '[';

    bool first = true;
    for (const auto& item : vec) {
      if (!first) {
        *out++ = ',';
        *out++ = ' ';
      }
      first = false;
      out = std::format_to(out, "{}", item);
    }

    *out++ = ']';
    return out;
  }
};

// Specialize std::formatter for StCollection to simplify debugging
template <> struct std::formatter<StCollection> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const StCollection& sc, format_context& ctx) const {
    return std::format_to(
        ctx.out(), "StCollection(id={}, sender_ids={}, ms_data_sizes={})",
        sc.id, sc.sender_ids, sc.ms_data_sizes);
  }
};

// Generic serialization utilities

template <typename T> std::vector<std::byte> to_bytes(const T& obj) {
  std::vector<char> char_buffer;
  char_buffer.reserve(1024);
  boost::iostreams::stream<
      boost::iostreams::back_insert_device<std::vector<char>>>
      stream(char_buffer);
  boost::archive::binary_oarchive archive(stream);
  archive << obj;
  stream.flush();
  // Fancy memcpy to convert char to std::byte
  std::vector<std::byte> result(char_buffer.size());
  std::transform(char_buffer.begin(), char_buffer.end(), result.begin(),
                 [](char c) { return std::bit_cast<std::byte>(c); });

  return result;
}

template <typename T> T to_obj(std::span<const std::byte> data) {
  boost::iostreams::stream<boost::iostreams::array_source> stream(
      reinterpret_cast<const char*>(data.data()), data.size());
  boost::archive::binary_iarchive archive(stream);
  T obj;
  archive >> obj;
  return obj;
}

template <typename T>
std::optional<T> to_obj_nothrow(std::span<const std::byte> data) noexcept {
  try {
    return to_obj<T>(data);
  } catch (const std::exception& e) {
    ERROR("Deserialization error: {}", e.what());
    return std::nullopt;
  }
}

// Wire (POD) serialization for StDescriptor and StCollection
//
// These structures sit on the hot path of the sender/scheduler/builder
// protocol; the generic Boost archive used for SenderInfo/BuilderInfo would
// require per-message heap traffic and is unnecessary for trivially-copyable
// payloads. The layout is a fixed POD header followed by packed arrays.
// All participants are assumed to share the same native endianness (x86_64).

namespace wire {

struct StDescriptorHeader {
  uint64_t start_time_ns;
  uint64_t duration_ns;
  uint32_t flags;
  uint32_t num_components;
};
static_assert(sizeof(StDescriptorHeader) == 24);
static_assert(std::is_trivially_copyable_v<StDescriptorHeader>);

struct StCollectionHeader {
  uint64_t id;
  uint32_t num_senders;
  uint32_t num_components;
  uint64_t merged_start_time_ns;
  uint64_t merged_duration_ns;
  uint32_t merged_flags;
  uint32_t total_sender_id_bytes;
};
static_assert(sizeof(StCollectionHeader) == 40);
static_assert(std::is_trivially_copyable_v<StCollectionHeader>);

} // namespace wire

inline std::vector<std::byte> serialize_descriptor(const StDescriptor& d) {
  wire::StDescriptorHeader h{};
  h.start_time_ns = d.start_time_ns;
  h.duration_ns = d.duration_ns;
  h.flags = d.flags;
  h.num_components = static_cast<uint32_t>(d.components.size());

  const std::size_t comp_bytes =
      d.components.size() * sizeof(StComponentDescriptor);
  std::vector<std::byte> out(sizeof(h) + comp_bytes);
  std::memcpy(out.data(), &h, sizeof(h));
  if (comp_bytes != 0) {
    std::memcpy(out.data() + sizeof(h), d.components.data(), comp_bytes);
  }
  return out;
}

inline std::optional<StDescriptor>
parse_descriptor(std::span<const std::byte> data) {
  if (data.size() < sizeof(wire::StDescriptorHeader)) {
    return std::nullopt;
  }
  wire::StDescriptorHeader h{};
  std::memcpy(&h, data.data(), sizeof(h));
  const std::size_t comp_bytes =
      h.num_components * sizeof(StComponentDescriptor);
  if (data.size() != sizeof(h) + comp_bytes) {
    return std::nullopt;
  }
  StDescriptor d;
  d.start_time_ns = h.start_time_ns;
  d.duration_ns = h.duration_ns;
  d.flags = h.flags;
  d.components.resize(h.num_components);
  if (comp_bytes != 0) {
    std::memcpy(d.components.data(), data.data() + sizeof(h), comp_bytes);
  }
  return d;
}

inline std::vector<std::byte> serialize_collection(const StCollection& c) {
  std::size_t sender_id_total = 0;
  for (const auto& s : c.sender_ids) {
    sender_id_total += s.size();
  }

  wire::StCollectionHeader h{};
  h.id = c.id;
  h.num_senders = static_cast<uint32_t>(c.sender_ids.size());
  h.num_components =
      static_cast<uint32_t>(c.merged_descriptor.components.size());
  h.merged_start_time_ns = c.merged_descriptor.start_time_ns;
  h.merged_duration_ns = c.merged_descriptor.duration_ns;
  h.merged_flags = c.merged_descriptor.flags;
  h.total_sender_id_bytes = static_cast<uint32_t>(sender_id_total);

  const std::size_t comp_bytes =
      h.num_components * sizeof(StComponentDescriptor);
  const std::size_t sizes_bytes = h.num_senders * sizeof(uint64_t);
  const std::size_t lens_bytes = h.num_senders * sizeof(uint32_t);
  std::vector<std::byte> out(sizeof(h) + comp_bytes + sizes_bytes + lens_bytes +
                             sender_id_total);

  std::byte* p = out.data();
  std::memcpy(p, &h, sizeof(h));
  p += sizeof(h);
  if (comp_bytes != 0) {
    std::memcpy(p, c.merged_descriptor.components.data(), comp_bytes);
    p += comp_bytes;
  }
  if (sizes_bytes != 0) {
    std::memcpy(p, c.ms_data_sizes.data(), sizes_bytes);
    p += sizes_bytes;
  }
  for (const auto& s : c.sender_ids) {
    auto len = static_cast<uint32_t>(s.size());
    std::memcpy(p, &len, sizeof(len));
    p += sizeof(len);
  }
  for (const auto& s : c.sender_ids) {
    std::memcpy(p, s.data(), s.size());
    p += s.size();
  }
  return out;
}

inline std::optional<StCollection>
parse_collection(std::span<const std::byte> data) {
  if (data.size() < sizeof(wire::StCollectionHeader)) {
    return std::nullopt;
  }
  wire::StCollectionHeader h{};
  std::memcpy(&h, data.data(), sizeof(h));
  const std::size_t comp_bytes =
      h.num_components * sizeof(StComponentDescriptor);
  const std::size_t sizes_bytes = h.num_senders * sizeof(uint64_t);
  const std::size_t lens_bytes = h.num_senders * sizeof(uint32_t);
  const std::size_t expected = sizeof(h) + comp_bytes + sizes_bytes +
                               lens_bytes + h.total_sender_id_bytes;
  if (data.size() != expected) {
    return std::nullopt;
  }

  StCollection c;
  c.id = h.id;
  c.merged_descriptor.start_time_ns = h.merged_start_time_ns;
  c.merged_descriptor.duration_ns = h.merged_duration_ns;
  c.merged_descriptor.flags = h.merged_flags;
  c.merged_descriptor.components.resize(h.num_components);

  const std::byte* p = data.data() + sizeof(h);
  if (comp_bytes != 0) {
    std::memcpy(c.merged_descriptor.components.data(), p, comp_bytes);
    p += comp_bytes;
  }
  c.ms_data_sizes.resize(h.num_senders);
  if (sizes_bytes != 0) {
    std::memcpy(c.ms_data_sizes.data(), p, sizes_bytes);
    p += sizes_bytes;
  }
  std::vector<uint32_t> lens(h.num_senders);
  if (lens_bytes != 0) {
    std::memcpy(lens.data(), p, lens_bytes);
    p += lens_bytes;
  }
  std::size_t sum = 0;
  for (auto l : lens) {
    sum += l;
  }
  if (sum != h.total_sender_id_bytes) {
    return std::nullopt;
  }
  c.sender_ids.reserve(h.num_senders);
  for (auto l : lens) {
    c.sender_ids.emplace_back(reinterpret_cast<const char*>(p), l);
    p += l;
  }
  return c;
}
