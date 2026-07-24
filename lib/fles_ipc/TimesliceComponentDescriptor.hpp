// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceComponentDescriptor serializable struct.
#pragma once

#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <cstdint>

namespace fles {

#pragma pack(1)

/**
 * \brief %Timeslice component descriptor struct.
 */
struct TimesliceComponentDescriptor {
  /// Global index of the corresponding timeslice. Note: this
  /// is not the same as the local index in the ring buffer.
  // Deprecated. Kept for now for backward compatibility.
  uint64_t ts_num;

  /// Start offset (in bytes) of the corresponding data
  /// (microslice descriptors followed by microslice contents)
  /// in the local data stream or ring buffer.
  // Unused, kept for backward compatibility. Use offset+ms_data_offset in
  // TimesliceShmWorkItem instead.
  uint64_t offset;

  /// Size (in bytes) of the corresponding data (microslice
  /// descriptors followed by microslice contents).
  uint64_t size;

  /// Number of microslices.
  uint64_t num_microslices;

  /// Flags for this component
  uint32_t flags;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & ts_num;
    ar & offset;
    ar & size;
    ar & num_microslices;
    if (version >= 1) {
      ar & flags;
    }
  }
};

#pragma pack()

} // namespace fles

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
BOOST_CLASS_VERSION(fles::TimesliceComponentDescriptor, 1)
#pragma GCC diagnostic pop
