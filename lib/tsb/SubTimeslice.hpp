// Copyright 2025 Dirk Hutter, Jan de Cuveland
#pragma once

#include "MicrosliceDescriptor.hpp"
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

// 2: sender -> builder and sender -> scheduler
//
// Descriptors for transferring subtimeslice data from the sender to the builder
// (and to the scheduler, for statistics)

struct DataRegion {
  std::ptrdiff_t offset;
  std::size_t size;

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & offset;
    ar & size;
  }
};

struct StComponentDescriptor {
  /// A data descriptor pointing to the microslice descriptor data blocks,
  /// followed by the microslice content data blocks
  DataRegion ms_data{};

  /// The number of microslices in this component
  std::size_t num_microslices = 0;

  /// Flags
  uint32_t flags = 0;

  void set_flag(TsComponentFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(TsComponentFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(TsComponentFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// The size (in bytes) of the component (i.e., microslice descriptor +
  /// content)
  [[nodiscard]] uint64_t ms_data_size() const { return ms_data.size; }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & ms_data;
    ar & flags;
  }
};

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
      total_ms_data_size += component.ms_data_size();
    }
    return total_ms_data_size;
  }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & start_time_ns;
    ar & duration_ns;
    ar & flags;
    ar & components;
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

  /// The total size (in bytes) of the collection (i.e., microslice
  /// descriptors + content of all contained components)
  [[nodiscard]] uint64_t ms_data_size() const {
    uint64_t total_ms_data_size = 0;
    for (const auto& size : ms_data_sizes) {
      total_ms_data_size += size;
    }
    return total_ms_data_size;
  }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & id;
    ar & sender_ids;
    ar & ms_data_sizes;
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

// 4: builder only
//
// Descriptor for storing timeslice data in a shared memory region

struct TsDescriptorShm {
  /// The UUID of the containing managed shared memory
  boost::uuids::uuid shm_uuid{};
  /// The identifier string of the containing managed shared memory
  std::string shm_identifier;
  /// The additional overall offset of all the data blocks
  std::ptrdiff_t offset = 0;
  /// Timeslice descriptor including component descriptors
  StDescriptor ts_desc{};

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & shm_uuid;
    ar & shm_identifier;
    ar & offset;
    ar & ts_desc;
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
