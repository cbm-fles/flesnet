// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "Microslice.hpp"
#include <boost/crc.hpp>
#include <cassert>

namespace fles
{

Microslice::~Microslice() {}

uint32_t Microslice::compute_crc() const
{
    assert(_content_ptr);
    assert(_desc_ptr);

    boost::crc_32_type crc_32;
    crc_32.process_bytes(_content_ptr, _desc_ptr->size);
    return crc_32();
};

bool Microslice::check_crc() const { return compute_crc() == _desc_ptr->crc; }

} // namespace fles {
