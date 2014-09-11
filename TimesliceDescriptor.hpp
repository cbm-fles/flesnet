// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>

namespace fles
{

#pragma pack(1)

//! Timeslice descriptor struct.
struct TimesliceDescriptor
{
    uint64_t index; ///< Global index of this timeslice
    uint64_t ts_pos; ///< Start offset (in items) of this timeslice
    uint32_t num_core_microslices; ///< Number of core microslices
    uint32_t num_components;       ///< Number of components (contributing input
    /// channels)

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        if (version > 0) {
            ar& index;
        }
        ar& ts_pos;
        ar& num_core_microslices;
        ar& num_components;
    }
};

#pragma pack()

} // namespace fles {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
BOOST_CLASS_VERSION(fles::TimesliceDescriptor, 1)
#pragma GCC diagnostic pop
