// Copyright 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceShmWorkItem serializable struct.
#pragma once

#include "TimesliceDescriptor.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
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
 * \brief %Timeslice shared memory work item struct.
 */
struct TimesliceShmWorkItem {
  /// The UUID of the containing managed shared memory
  boost::uuids::uuid shm_uuid{};
  /// The identifier string of the containing managed shared memory
  std::string shm_identifier;
  /// The timeslice descriptor
  TimesliceDescriptor ts_desc{};
  /// A vector of handles to the data blocks
  std::vector<std::ptrdiff_t> ms_data_offset;

  // Legacy member kept for backward compatibility

  /// A vector of handles to the tsc descriptor blocks
  std::vector<std::ptrdiff_t> desc;

  // New members (replaces the legacy desc member)

  /// Sizes of the data blocks
  std::vector<std::size_t> ms_data_size;
  /// Number of microslices in each component
  std::vector<std::size_t> num_microslices;
  /// Flags of each component
  std::vector<uint32_t> component_flags;
  /// The additional overall offset of all the data blocks
  std::ptrdiff_t offset = 0;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & shm_uuid;
    ar & shm_identifier;
    ar & ts_desc;
    ar & ms_data_offset;
    if (version == 0) {
      ar & desc;
    }
    if (version > 0) {
      ar & ms_data_size;
      ar & num_microslices;
      ar & component_flags;
      ar & offset;
    }
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const TimesliceShmWorkItem& i) {
    return os << "TimesliceShmWorkItem(shm_uuid=" << i.shm_uuid
              << ", shm_identifier=" << i.shm_identifier
              << ", ts_desc=" << i.ts_desc << ", ...)";
  }
};

#pragma pack()

} // namespace fles

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
BOOST_CLASS_VERSION(fles::TimesliceShmWorkItem, 1)
#pragma GCC diagnostic pop
