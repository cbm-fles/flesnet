// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceDescriptor serializable struct.
#pragma once

#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <cstdint>
#include <ostream>

namespace fles {

#pragma pack(1)

/**
 * \brief %Timeslice descriptor struct.
 */
struct TimesliceDescriptor {
  /// Global identifier of the timeslice, unique during a run.
  uint64_t index = 0;

  /// The start time of the subtimeslice in nanoseconds, should be divisible by
  /// the duration
  uint64_t start_time = 0;

  /// The duration of the timeslice in nanoseconds
  uint64_t duration = 0;

  /// The flags of the timeslice
  uint32_t flags = 0;

  /// Start offset (in items) of the timeslice in the local data stream or ring
  /// buffer.
  // Deprecated. Kept for now for backward compatibility.
  uint64_t ts_pos = 0;

  /// Number of core microslices.
  // Deprecated. Kept for now for backward compatibility.
  uint32_t num_core_microslices = 0;

  /// Number of components (contributing input channels)
  uint32_t num_components = 0;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    if (version > 0) {
      ar & index;
    }
    if (version > 1) {
      ar & start_time;
      ar & duration;
      ar & flags;
    }
    ar & ts_pos;
    ar & num_core_microslices;
    ar & num_components;
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const TimesliceDescriptor& d) {
    return os << "TimesliceDescriptor(index=" << d.index
              << ", num_components=" << d.num_components << ")";
  }
};

#pragma pack()

} // namespace fles

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
BOOST_CLASS_VERSION(fles::TimesliceDescriptor, 2)
#pragma GCC diagnostic pop
