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
#include <log.hpp>
#include <span>
#include <string>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <vector>

using TsID = uint64_t;

// Flags

enum class StComponentFlag : uint32_t {
  None = 0,

  // Microslices are missing in this component
  MissingMicroslices = 1 << 0,

  // One or more microslices in this component have their "OverflowFlim" flag
  // set
  OverflowFlim = 1 << 1
};

enum class StFlag : uint32_t {
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

// Descriptors for transferring subtimeslice data from the sender to the builder

struct DataDescriptor {
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
  /// A data descriptor pointing to the microslice descriptor data blocks
  DataDescriptor descriptor{};

  /// A data descriptor pointing to the microslice content data blocks
  DataDescriptor content{};

  // Flags
  uint32_t flags = 0;

  void set_flag(StComponentFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(StComponentFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(StComponentFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  // The size (in bytes) of the component (i.e., microslice descriptor +
  // content)
  [[nodiscard]] uint64_t size() const { return descriptor.size + content.size; }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & descriptor;
    ar & content;
    ar & flags;
  }
};

struct StDescriptor {
  /// The start time of the subtimeslice in nanoseconds, should be divisible by
  /// the duration (duration_ns)
  uint64_t start_time_ns;

  /// The duration of the subtimeslice in nanoseconds
  uint64_t duration_ns;

  // Flags
  uint32_t flags = 0;

  /// The subtimeslice component descriptors
  std::vector<StComponentDescriptor> components;

  void set_flag(StFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(StFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(StFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// The total size (in bytes) of the subtimeslice (i.e., microslice
  /// descriptors + content of all contained components)
  [[nodiscard]] uint64_t size() const {
    uint64_t total_size = 0;
    for (const auto& component : components) {
      total_size += component.size();
    }
    return total_size;
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

// Internal structures for transferring subtimeslice memory handles to the
// StSender

struct StComponentHandle {
  std::vector<ucp_dt_iov> descriptors;
  std::vector<ucp_dt_iov> contents;
  uint32_t flags = 0;

  void set_flag(StComponentFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(StComponentFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(StComponentFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// The number of microslices, computed from the size of the microslice
  /// descriptor data blocks
  [[nodiscard]] uint32_t num_microslices() const {
    return descriptors_size() / sizeof(fles::MicrosliceDescriptor);
  }

  /// The number of descriptor bytes
  [[nodiscard]] uint64_t descriptors_size() const {
    uint64_t descriptors_size = 0;
    for (const auto& sg : descriptors) {
      descriptors_size += sg.length;
    }
    return descriptors_size;
  }

  /// The number of content bytes
  [[nodiscard]] uint64_t contents_size() const {
    uint64_t contents_size = 0;
    for (const auto& sg : contents) {
      contents_size += sg.length;
    }
    return contents_size;
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const StComponentHandle& i) {
    return os << "StComponentHandle(num_microslices=" << i.num_microslices()
              << ", flags=" << i.flags << ")";
  }
};

struct SubTimesliceHandle {
  uint64_t start_time_ns;
  uint64_t duration_ns;
  uint32_t flags;
  std::vector<StComponentHandle> components;

  void set_flag(StFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(StFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(StFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// Dump contents (for debugging)
  friend std::ostream& operator<<(std::ostream& os,
                                  const SubTimesliceHandle& i) {
    return os << "SubTimesliceHandle(start_time_ns=" << i.start_time_ns
              << ", duration_ns=" << i.duration_ns << ", flags=" << i.flags
              << ", components=...)";
  }
};

// Descriptors for transferring subtimeslice data from the sender to the builder

struct TsContribution {
  std::string sender_id; // ID of the sender
  uint64_t desc_size;    // Size of the descriptor data
  uint64_t content_size; // Size of the content data

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & sender_id;
    ar & desc_size;
    ar & content_size;
  }
};

struct StCollectionDescriptor {
  TsID id = 0;
  uint64_t desc_size = 0;
  uint64_t content_size = 0;

  std::vector<TsContribution> contributions;

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar & id;
    ar & contributions;
  }
};

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
    L_(error) << "Deserialization error: " << e.what();
    return std::nullopt;
  }
}