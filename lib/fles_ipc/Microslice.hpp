// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::Microslice abstract base class.
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
 * \brief The Microslice class provides read access to the data of a microslice.
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

    /// Compute CRC-32 checksum of microslice data content
    uint32_t compute_crc() const;

    /// Compare computed CRC-32 checksum to value in header
    bool check_crc() const;

protected:
    Microslice(){};

    /// Construct microslice with given content.
    Microslice(const MicrosliceDescriptor* desc_ptr, const uint8_t* content_ptr)
        : _desc_ptr(desc_ptr), _content_ptr(content_ptr){};

    friend class StorableMicroslice;

    /// Pointer to the microslice descriptor
    const MicrosliceDescriptor* _desc_ptr;

    /// Pointer to the microslice data content
    const uint8_t* _content_ptr;
};

} // namespace fles {
