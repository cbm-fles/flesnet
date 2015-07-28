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
    assert(content_ptr_);
    assert(desc_ptr_);

    boost::crc_optimal<32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true, true>
        crc_32;
    crc_32.process_bytes(content_ptr_, desc_ptr_->size);
    return crc_32();
}

bool Microslice::check_crc() const { return compute_crc() == desc_ptr_->crc; }

} // namespace fles {
