// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>
#include <boost/serialization/access.hpp>

namespace fles
{

#pragma pack(1)

//! Timeslice descriptor struct.
struct TimesliceDescriptor
{
    uint64_t ts_pos; ///< Start offset (in items) of this timeslice
    uint32_t num_core_microslices; ///< Number of core microslices
    uint32_t num_components;       ///< Number of components (contributing input
    /// channels)

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& ts_pos;
        ar& num_core_microslices;
        ar& num_components;
    }
};

#pragma pack()

} // namespace fles {
