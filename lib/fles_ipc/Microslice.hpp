// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include <fstream>
#include <boost/serialization/access.hpp>
// Note: <fstream> has to precede boost/serialization includes for non-obvious
// reasons to avoid segfault similar to
// http://lists.debian.org/debian-hppa/2009/11/msg00069.html

namespace fles
{

/**
 * @brief The Microslice class provides read access to the data of a microslice.
 *
 * This class is an abstract base class for all classes providing access to the
 * descriptor and data contents of a single microslice.
 */
class Microslice
{
public:
    virtual ~Microslice() = 0;

    /// Retrieve microslice descriptor
    const MicrosliceDescriptor& desc() const { return *_desc_ptr; }

    /// Retrieve a pointer to the microslice data
    const uint8_t* content() const { return _content_ptr; }

protected:
    Microslice(){};
    Microslice(const MicrosliceDescriptor* desc_ptr, const uint8_t* content_ptr)
        : _desc_ptr(desc_ptr), _content_ptr(content_ptr){};

    friend class StorableMicroslice;

    const MicrosliceDescriptor* _desc_ptr;
    const uint8_t* _content_ptr;
};

} // namespace fles {
