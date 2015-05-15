// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceView.hpp"
#include "MicrosliceDescriptor.hpp"
#include "MicrosliceComponentDescriptor.hpp"
#include <vector>
#include <fstream>
#include <boost/serialization/access.hpp>
// Note: <fstream> has to precede boost/serialization includes for non-obvious
// reasons to avoid segfault similar to
// http://lists.debian.org/debian-hppa/2009/11/msg00069.html

namespace fles
{

//! Microslice is the base class for all classes providing access to the data of
// a single microslice.
class Microslice
{
public:
    virtual ~Microslice() = 0;

    /// Retrieve microslice descriptor
    const MicrosliceDescriptor& desc() const
    {
        return *_desc;
    }

    /// Retrieve a pointer to the microslice data
    const uint8_t* content() const
    {
        return _content;
    }

protected:
    Microslice() {};

    friend class StorableMicroslice;

    const MicrosliceComponentDescriptor* _desc_ptr;
    const uint8_t* _content_ptr;
};

} // namespace fles {
