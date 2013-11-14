// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceView.hpp"
#include "MicrosliceDescriptor.hpp"
#include "TimesliceWorkItem.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include <cstdint>
#include <vector>
#include <fstream>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
// Note: <fstream> has to precede boost/serialization includes for non-obvious
// reasons to avoid segfault similar to
// http://lists.debian.org/debian-hppa/2009/11/msg00069.html

namespace fles
{

#pragma pack(1)

//! The StorableTimeslice class contains the data of a single timeslice.
class StorableTimeslice
{
public:
    StorableTimeslice(const TimesliceView& ts);

    StorableTimeslice(const StorableTimeslice& other) = delete;
    void operator=(const StorableTimeslice&) = delete;

    /// Retrieve the timeslice index.
    uint64_t index() const
    {
        return _desc_ptr[0]->ts_num;
    }

    /// Retrieve the number of core microslices.
    uint64_t num_core_microslices() const
    {
        return _work_item.ts_desc.num_core_microslices;
    }

    /// Retrieve the total number of microslices.
    uint64_t num_microslices(uint64_t component) const
    {
        return _desc_ptr[component]->num_microslices;
    }

    /// Retrieve the number of components (contributing input channels).
    uint64_t num_components() const
    {
        return _work_item.ts_desc.num_components;
    }

    /// Retrieve a pointer to the data content of a given microslice
    const uint8_t* content(uint64_t component, uint64_t microslice) const
    {
        return _data_ptr[component] + _desc_ptr[component]->num_microslices
                                      * sizeof(MicrosliceDescriptor)
               + descriptor(component, microslice).offset
               - descriptor(component, 0).offset;
    }

    /// Retrieve the descriptor of a given microslice
    const MicrosliceDescriptor& descriptor(uint64_t component,
                                           uint64_t microslice) const
    {
        return (reinterpret_cast<const MicrosliceDescriptor*>(
            _data_ptr[component]))[microslice];
    }

    void init_pointers()
    {
        _data_ptr.resize(num_components());
        _desc_ptr.resize(num_components());
        for (size_t c = 0; c < num_components(); ++c) {
            _desc_ptr[c] = &_desc[c];
            _data_ptr[c] = _data[c].data();
        }
    }

private:
    friend class boost::serialization::access;
    friend class TimesliceInputArchive;

    StorableTimeslice();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& _work_item;
        ar& _data;
        ar& _desc;

        init_pointers();
    }

    TimesliceWorkItem _work_item;
    std::vector<std::vector<uint8_t> > _data;
    std::vector<TimesliceComponentDescriptor> _desc;

    std::vector<const uint8_t*> _data_ptr;
    std::vector<const TimesliceComponentDescriptor*> _desc_ptr;
};

} // namespace fles {
