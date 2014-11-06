// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>
#include <boost/serialization/access.hpp>

namespace fles
{

#pragma pack(1)

//! Timeslice completion struct.
struct TimesliceCompletion
{
    uint64_t ts_pos; ///< Start offset (in items) of this timeslice

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& ts_pos;
    }
};

#pragma pack()

} // namespace fles {
