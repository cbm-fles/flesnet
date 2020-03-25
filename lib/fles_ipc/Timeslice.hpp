// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::Timeslice abstract base class.
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "MicrosliceView.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include <fstream>
#include <vector>

#include <boost/serialization/access.hpp>
// Note: <fstream> has to precede boost/serialization includes for non-obvious
// reasons to avoid segfault similar to
// http://lists.debian.org/debian-hppa/2009/11/msg00069.html

namespace fles {

/**
 * \brief The Timeslice class provides read access to the data of a timeslice.
 *
 * This class is an abstract base class for all classes providing access to the
 * contents of a single timeslice.
 */
class Timeslice {
public:
  virtual ~Timeslice() = 0;

  /// Retrieve the timeslice index.
  uint64_t index() const { return timeslice_descriptor_.index; }

  /// Retrieve the number of core microslices.
  uint64_t num_core_microslices() const {
    return timeslice_descriptor_.num_core_microslices;
  }

  /// Retrieve the total number of microslices.
  uint64_t num_microslices(uint64_t component) const {
    return desc_ptr_[component]->num_microslices;
  }

  /// Retrieve the number of components (contributing input channels).
  uint64_t num_components() const {
    return timeslice_descriptor_.num_components;
  }

  /// Retrieve the size of a given component.
  uint64_t size_component(uint64_t component) const {
    return desc_ptr_[component]->size;
  }

  /// Retrieve a pointer to the data content of a given microslice
  const uint8_t* content(uint64_t component, uint64_t microslice) const {
    return data_ptr_[component] +
           desc_ptr_[component]->num_microslices *
               sizeof(MicrosliceDescriptor) +
           descriptor(component, microslice).offset -
           descriptor(component, 0).offset;
  }

  /// Retrieve the descriptor of a given microslice
  const MicrosliceDescriptor& descriptor(uint64_t component,
                                         uint64_t microslice) const {
    return reinterpret_cast<const MicrosliceDescriptor*>(
        data_ptr_[component])[microslice];
  }

  /// Retrieve the descriptor and pointer to the data of a given microslice
  MicrosliceView get_microslice(uint64_t component,
                                uint64_t microslice_index) const {
    uint8_t* component_data_ptr = data_ptr_[component];

    MicrosliceDescriptor& dd = reinterpret_cast<MicrosliceDescriptor*>(
        component_data_ptr)[microslice_index];

    MicrosliceDescriptor& dd0 =
        reinterpret_cast<MicrosliceDescriptor*>(component_data_ptr)[0];

    uint8_t* cc =
        component_data_ptr +
        desc_ptr_[component]->num_microslices * sizeof(MicrosliceDescriptor) +
        dd.offset - dd0.offset;

    return MicrosliceView(dd, cc);
  }

protected:
  Timeslice() = default;

  friend class StorableTimeslice;

  /// The timeslice descriptor.
  TimesliceDescriptor timeslice_descriptor_;

  /// A vector of pointers to the data content, one per timeslice component.
  std::vector<uint8_t*> data_ptr_;

  /// \brief A vector of pointers to the microslice descriptors, one per
  /// timeslice component.
  std::vector<TimesliceComponentDescriptor*> desc_ptr_;
};

} // namespace fles
