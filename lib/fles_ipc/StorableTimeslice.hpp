// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

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
    StorableTimeslice(const Timeslice& ts);

    StorableTimeslice(const StorableTimeslice& other) = delete;
    void operator=(const StorableTimeslice&) = delete;

private:
    friend class boost::serialization::access;
    friend class TimesliceInputArchive;

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
