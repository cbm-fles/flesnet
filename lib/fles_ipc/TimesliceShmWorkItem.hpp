// Copyright 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceShmWorkItem serializable struct.
#pragma once

#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include <boost/archive/archive_exception.hpp>
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
 *
 * This is the payload of the work items published to the item workers. It is
 * never stored, so only the current version is accepted.
 */
struct TimesliceShmWorkItem {
  /// The UUID of the containing managed shared memory
  boost::uuids::uuid shm_uuid;
  /// The identifier string of the containing managed shared memory
  std::string shm_identifier;
  /// The timeslice descriptor
  TimesliceDescriptor ts_desc{};
  /// A vector of handles to the data blocks
  std::vector<std::ptrdiff_t> data;

  /// A vector of timeslice component descriptors
  std::vector<TimesliceComponentDescriptor> tsc_desc;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    if (version != 2) {
      throw boost::archive::archive_exception(
          boost::archive::archive_exception::unsupported_class_version,
          "fles::TimesliceShmWorkItem");
    }
    ar & shm_uuid;
    ar & shm_identifier;
    ar & ts_desc;
    ar & data;
    ar & tsc_desc;
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
BOOST_CLASS_VERSION(fles::TimesliceShmWorkItem, 2)
#pragma GCC diagnostic pop
