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
  /// Global index of the timeslice. Monotonically increasing during a run,
  /// relates to the timestamps of the data contents. Note: this is not the same
  /// as the local index in the ring buffer.
  uint64_t index;

  /// Start offset (in items) of the timeslice in the local data stream or ring
  /// buffer.
  uint64_t ts_pos;

  /// Number of core microslices
  uint32_t num_core_microslices;

  /// Number of components (contributing input channels)
  uint32_t num_components;

  friend class boost::serialization::access;
  /// Provide boost serialization access.
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    if (version > 0) {
      ar& index;
    }
    ar& ts_pos;
    ar& num_core_microslices;
    ar& num_components;
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const TimesliceDescriptor& d) {
    return os << "TimesliceDescriptor(index=" << d.index
              << ", ts_pos=" << d.ts_pos
              << ", num_core_microslices=" << d.num_core_microslices
              << ", num_components=" << d.num_components << ")";
  }
};

#pragma pack()

} // namespace fles

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
BOOST_CLASS_VERSION(fles::TimesliceDescriptor, 1)
#pragma GCC diagnostic pop
