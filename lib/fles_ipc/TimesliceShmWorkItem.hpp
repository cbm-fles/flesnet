// Copyright 2020 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceShmWorkItem serializable struct.
#pragma once

#include "TimesliceDescriptor.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <cstddef>
#include <vector>

namespace fles {

#pragma pack(1)

/**
 * \brief %Timeslice shared memory work item struct.
 */
struct TimesliceShmWorkItem {
  /// The timeslice descriptor
  TimesliceDescriptor ts_desc;
  /// A vector of handles to the data blocks
  std::vector<std::ptrdiff_t> data;
  /// A vector of handles to the tsc descriptor blocks
  std::vector<std::ptrdiff_t> desc;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar& ts_desc;
    ar& data;
    ar& desc;
  }
};

#pragma pack()

} // namespace fles
