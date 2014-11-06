// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "TimesliceDescriptor.hpp"
#include <cstdint>
#include <boost/serialization/access.hpp>

namespace fles
{

#pragma pack(1)

//! Timeslice work item struct.
struct TimesliceWorkItem
{
    TimesliceDescriptor ts_desc;
    uint32_t data_buffer_size_exp; ///< Exp. size (in bytes) of each data buffer
    uint32_t desc_buffer_size_exp; ///< Exp. size (in bytes) of each descriptor
    /// buffer

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& ts_desc;
        ar& data_buffer_size_exp;
        ar& desc_buffer_size_exp;
    }
};

#pragma pack()

} // namespace fles {
