// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Timeslice.hpp"
#include "MicrosliceDescriptor.hpp"

class TimesliceDebugger
{
public:
    std::string dump_timeslice(const fles::Timeslice& ts,
                               size_t verbosity) const;
    std::string
    dump_microslice_descriptor(const fles::MicrosliceDescriptor& md) const;
    std::string dump_buffer(const void* buf, uint64_t size) const;
    std::string dump_microslice(const fles::MicrosliceDescriptor& md,
                                const void* buf) const;
};
