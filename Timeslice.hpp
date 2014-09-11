// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceView.hpp"
#include "MicrosliceDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <vector>
#include <fstream>
#include <boost/serialization/access.hpp>
// Note: <fstream> has to precede boost/serialization includes for non-obvious
// reasons to avoid segfault similar to
// http://lists.debian.org/debian-hppa/2009/11/msg00069.html

namespace fles
{

//! Timeslice is the base class for all classes providing access to the data of
// a single timeslice.
class Timeslice
{
public:
    virtual ~Timeslice() = 0;

    /// Retrieve the timeslice index.
    uint64_t index() const
    {
        return _timeslice_descriptor.index;
    }

    /// Retrieve the number of core microslices.
    uint64_t num_core_microslices() const
    {
        return _timeslice_descriptor.num_core_microslices;
    }

    /// Retrieve the total number of microslices.
    uint64_t num_microslices(uint64_t component) const
    {
        return _desc_ptr[component]->num_microslices;
    }

    /// Retrieve the number of components (contributing input channels).
    uint64_t num_components() const
    {
        return _timeslice_descriptor.num_components;
    }

    /// Retrieve a pointer to the data content of a given microslice
    const uint8_t* content(uint64_t component, uint64_t microslice) const
    {
        return _data_ptr[component] + _desc_ptr[component]->num_microslices *
                                          sizeof(MicrosliceDescriptor) +
               descriptor(component, microslice).offset -
               descriptor(component, 0).offset;
    }

    /// Retrieve the descriptor of a given microslice
    const MicrosliceDescriptor& descriptor(uint64_t component,
                                           uint64_t microslice) const
    {
        return reinterpret_cast<const MicrosliceDescriptor*>(
            _data_ptr[component])[microslice];
    }

    /// Retrieve the descriptor and pointer to the data of a given microslice
    MicrosliceView get_microslice(uint64_t component, uint64_t mc_index) const
    {
        return {descriptor(component, mc_index), content(component, mc_index)};
    }

protected:
    Timeslice() {};

    friend class StorableTimeslice;

    TimesliceDescriptor _timeslice_descriptor;
    std::vector<const uint8_t*> _data_ptr;
    std::vector<const TimesliceComponentDescriptor*> _desc_ptr;
};

} // namespace fles {
