// Copyright 2025 Dirk Hutter, Jan de Cuveland
#pragma once

#include "MicrosliceDescriptor.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <sstream>
#include <string>
#include <ucp/api/ucp.h>
#include <vector>

// Descriptors for transferring subtimeslice data to the builder

struct DataDescriptor {
  std::ptrdiff_t offset;
  std::size_t size;

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & offset;
    ar & size;
  }
};

struct StComponentDescriptor {
  /// A data descriptor pointing to the microslice descriptor data blocks
  DataDescriptor descriptor{};

  /// A data descriptor pointing to the microslice content data blocks
  DataDescriptor content{};

  // Flag: Microslices are missing in this component
  bool is_missing_microslices = false;

  // Flag: one or more of the microslices in this component have their
  // "truncate" flag set
  // bool contains_incomplete_microslices; // TODO: Future addition

  [[nodiscard]] uint64_t size() const { return descriptor.size + content.size; }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & descriptor;
    ar & content;
    ar & is_missing_microslices;
  }
};

struct StDescriptor {
  /// The start time of the subtimeslice in nanoseconds, should be divisible by
  /// the duration (duration_ns)
  uint64_t start_time_ns;

  /// The duration of the subtimeslice in nanoseconds
  uint64_t duration_ns;

  /// Flag to indicate that components are missing in this SubTimeslice, e. g.
  /// due to a failed entry node
  bool is_incomplete;

  /// The subtimeslice component descriptors
  std::vector<StComponentDescriptor> components;

  [[nodiscard]] uint64_t size() const {
    uint64_t total_size = 0;
    for (const auto& component : components) {
      total_size += component.size();
    }
    return total_size;
  }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & start_time_ns;
    ar & duration_ns;
    ar & is_incomplete;
    ar & components;
  }

  [[nodiscard]] std::string to_string() const {
    std::ostringstream oss;
    boost::archive::binary_oarchive oa(oss);
    oa << *this;
    return oss.str();
  }
};

// Internal structures for transferring subtimeslice memory handles to the
// StSender

struct StComponentHandle {
  std::vector<ucp_dt_iov> descriptors;
  std::vector<ucp_dt_iov> contents;
  bool is_missing_microslices;

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
              << ", is_missing_microslices=" << i.is_missing_microslices << ")";
  }
};

struct SubTimesliceHandle {
  uint64_t start_time_ns;
  uint64_t duration_ns;
  bool is_incomplete;
  std::vector<StComponentHandle> components;

  /// Dump contents (for debugging)
  friend std::ostream& operator<<(std::ostream& os,
                                  const SubTimesliceHandle& i) {
    return os << "SubTimesliceHandle(start_time_ns=" << i.start_time_ns
              << ", duration_ns=" << i.duration_ns
              << ", is_incomplete=" << i.is_incomplete << ", components=...)";
  }
};

#pragma pack()
