// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "Microslice.hpp"
#include <boost/crc.hpp>
#include <cassert>

namespace fles
{

Microslice::~Microslice() {}

/// This function points to a scalar reference implementation of the CRC-32C
/// algorithm that yields the correct value. It is optimized for portability,
/// not for speed on modern architectures.
uint32_t Microslice::compute_crc() const
{
    assert(_content_ptr);
    assert(_desc_ptr);

    boost::crc_optimal<32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true, true>
        crc_32;
    crc_32.process_bytes(_content_ptr, _desc_ptr->size);
    return crc_32();
}

bool Microslice::check_crc() const { return compute_crc() == _desc_ptr->crc; }

} // namespace fles {
