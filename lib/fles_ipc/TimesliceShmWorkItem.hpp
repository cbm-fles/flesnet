// Copyright 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceShmWorkItem serializable struct.
#pragma once

#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/version.hpp>
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
  std::vector<std::ptrdiff_t> data;

  // Legacy member kept for backward compatibility

  /// A vector of handles to the tsc descriptor blocks
  std::vector<std::ptrdiff_t> desc;

  // New members (replaces the legacy desc member)

  /// A vector of timeslice component descriptors
  std::vector<TimesliceComponentDescriptor> tsc_desc;
  /// The additional overall offset of all the data blocks
  std::ptrdiff_t offset = 0;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & shm_uuid;
    ar & shm_identifier;
    ar & ts_desc;
    ar & data;
    ar & desc;
    if (version > 0) {
      ar & tsc_desc;
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
