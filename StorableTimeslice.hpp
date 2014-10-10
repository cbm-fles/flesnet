// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "StorableMicroslice.hpp"
#include "Timeslice.hpp"
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

//! The StorableTimeslice class contains the data of a single timeslice.
class StorableTimeslice : public Timeslice
{
public:
    StorableTimeslice(const StorableTimeslice& ts);
    void operator=(const StorableTimeslice&) = delete;
    StorableTimeslice(StorableTimeslice&& ts);

    StorableTimeslice(const Timeslice& ts);

    StorableTimeslice(uint32_t num_core_microslices,
                      uint64_t index = UINT64_MAX,
                      uint64_t ts_pos = UINT64_MAX)
    {
        _timeslice_descriptor.index = index;
        _timeslice_descriptor.ts_pos = ts_pos;
        _timeslice_descriptor.num_core_microslices = num_core_microslices;
        _timeslice_descriptor.num_components = 0;
    }

    uint32_t append_component(uint64_t num_microslices, uint64_t /* dummy */ = 0)
    {
        TimesliceComponentDescriptor ts_desc = TimesliceComponentDescriptor();
        ts_desc.ts_num = _timeslice_descriptor.index;
        ts_desc.offset = 0;
        ts_desc.num_microslices = num_microslices;

        std::vector<uint8_t> data;
        for (uint64_t m = 0; m < num_microslices; ++m) {
            MicrosliceDescriptor desc = MicrosliceDescriptor();
            uint8_t* desc_bytes = reinterpret_cast<uint8_t*>(&desc);
            data.insert(data.end(), desc_bytes,
                        desc_bytes + sizeof(MicrosliceDescriptor));
        }

        ts_desc.size = data.size();
        _desc.push_back(ts_desc);
        _data.push_back(data);
        uint32_t component = _timeslice_descriptor.num_components++;

        init_pointers();
        return component;
    }

    uint32_t append_microslice(uint64_t component, uint64_t microslice,
                               MicrosliceDescriptor& descriptor,
                               const uint8_t* content)
    {
        assert(component < _timeslice_descriptor.num_components);
        std::vector<uint8_t>& this_data = _data[component];
        TimesliceComponentDescriptor& this_desc = _desc[component];

        assert(microslice < this_desc.num_microslices);
        uint8_t* desc_bytes = reinterpret_cast<uint8_t*>(&descriptor);

        // set offset relative to first microslice
        if (microslice > 0) {
            uint64_t offset =
                this_data.size() -
                this_desc.num_microslices * sizeof(MicrosliceDescriptor);
            uint64_t first_offset = reinterpret_cast<MicrosliceDescriptor*>(
                                        this_data.data())->offset;
            descriptor.offset = offset + first_offset;
        }

        std::copy(desc_bytes, desc_bytes + sizeof(MicrosliceDescriptor),
                  &this_data[microslice * sizeof(MicrosliceDescriptor)]);

        this_data.insert(this_data.end(), content, content + descriptor.size);
        this_desc.size = this_data.size();

        init_pointers();
        return microslice;
    }

    uint32_t append_microslice(uint64_t component, uint64_t microslice,
                               StorableMicroslice& mc)
    {
        return append_microslice(component, microslice, mc.desc(), mc.content());
    }

private:
    friend class boost::serialization::access;
    friend class TimesliceInputArchive;
    friend class TimesliceSubscriber;

    StorableTimeslice();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& _timeslice_descriptor;
        ar& _data;
        ar& _desc;

        init_pointers();
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

    std::vector<std::vector<uint8_t>> _data;
    std::vector<TimesliceComponentDescriptor> _desc;
};

} // namespace fles {
