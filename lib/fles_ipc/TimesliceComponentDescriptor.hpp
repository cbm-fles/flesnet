// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceComponentDescriptor serializable struct.
#pragma once

#include <boost/serialization/access.hpp>
#include <cstdint>

namespace fles {

#pragma pack(1)

/**
 * \brief %Timeslice component descriptor struct.
 */
struct TimesliceComponentDescriptor {
  /// Global index of the corresponding timeslice. Note: this
  /// is not the same as the local index in the ring buffer.
  uint64_t ts_num;

  /// Start offset (in bytes) of the corresponding data
  /// (microslice descriptors followed by microslice contents)
  /// in the local data stream or ring buffer.
  uint64_t offset;

  /// Size (in bytes) of the corresponding data (microslice
  /// descriptors followed by microslice contents).
  uint64_t size;

  /// Number of microslices.
  uint64_t num_microslices;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar& ts_num;
    ar& offset;
    ar& size;
    ar& num_microslices;
  }
};

#pragma pack()

} // namespace fles
