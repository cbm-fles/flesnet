// Copyright 2025 Dirk Hutter, Jan de Cuveland
/// \file
/// \brief Defines the fles::SubTimesliceDescriptor serializable struct.
#pragma once

#include "MicrosliceDescriptor.hpp"
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_serialize.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace fles {

#pragma pack(1)

/**
 * \brief %Shared memory IO vector struct. Similar to C struct iovec, but with
 * offsets instead of pointers.
 */
struct ShmIovec {
  std::ptrdiff_t offset;
  std::size_t size;
};

/**
 * \brief Subtimeslice component descriptor struct.
 */
struct SubTimesliceComponentDescriptor {
  /// A vector of IO vectors to the microslice content data blocks in the shared
  /// memory
  std::vector<ShmIovec> contents;

  /// A vector of IO vectors to the microslice descriptor data blocks in the
  /// shared memory
  std::vector<ShmIovec> descriptors;

  // Flag: Microslices are missing in this component.
  bool is_missing_microslices;

  // Flag: one or more of the microslices in this component have their
  // "truncate" flag set
  // bool contains_incomplete_microslices; // TODO: Future addition

  /// The number of microslices, computed from the size of the microslice
  /// descriptor data blocks
  [[nodiscard]] uint32_t num_microslices() const {
    std::size_t descriptors_size = 0;
    for (const auto& sg : descriptors) {
      descriptors_size += sg.size;
    }
    return descriptors_size / sizeof(fles::MicrosliceDescriptor);
  }

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & contents;
    ar & descriptors;
    ar & is_missing_microslices;
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const SubTimesliceComponentDescriptor& i) {
    return os << "SubTimesliceComponentDescriptor(num_microslices="
              << i.num_microslices()
              << ", is_missing_microslices=" << i.is_missing_microslices << ")";
  }
};

/**
 * \brief Subtimeslice descriptor struct.
 */
struct SubTimesliceDescriptor {
  /// The identifier string of the containing managed shared memory
  std::string shm_identifier;

  /// The UUID of the containing managed shared memory
  boost::uuids::uuid shm_uuid{};

  /// The start time of the subtimeslice in nanoseconds, should be divisible by
  /// the duration (duration_ns)
  uint64_t start_time_ns;

  /// The duration of the subtimeslice in nanoseconds
  uint64_t duration_ns;

  /// Flag to indicate that components are missing in this SubTimeslice, e. g.
  /// due to a failed entry node
  bool is_incomplete;

  /// The subtimeslice component descriptors
  std::vector<SubTimesliceComponentDescriptor> components;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & shm_identifier;
    ar & shm_uuid;
    ar & start_time_ns;
    ar & is_incomplete;
    ar & duration_ns;
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const SubTimesliceDescriptor& i) {
    return os << "SubTimesliceDescriptor(shm_identifier=" << i.shm_identifier
              << ", shm_uuid=" << i.shm_uuid
              << ", start_time_ns=" << i.start_time_ns
              << ", duration_ns=" << i.duration_ns
              << ", is_incomplete=" << i.is_incomplete << ", components=...)";
  }
};

#pragma pack()

} // namespace fles
