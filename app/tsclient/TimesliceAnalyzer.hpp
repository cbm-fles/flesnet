// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Timeslice.hpp"
#include "MicrosliceDescriptor.hpp"

class TimesliceAnalyzer
{
public:
    bool check_timeslice(const fles::Timeslice& ts);

    std::string statistics() const;
    void reset()
    {
        _microslice_count = 0;
        _content_bytes = 0;
    }

    size_t count() const { return _timeslice_count; }

private:
    bool check_flesnet_pattern(const fles::MicrosliceDescriptor& descriptor,
                               const uint64_t* content, size_t component);

    bool check_content_pgen(const uint16_t* content, size_t size);

    bool check_cbmnet_frames(const uint16_t* content, size_t size,
                             uint8_t sys_id, uint8_t sys_ver);

    bool check_flib_pattern(const fles::MicrosliceDescriptor& descriptor,
                            const uint64_t* content, size_t /* component */);

    bool check_microslice(const fles::MicrosliceDescriptor& descriptor,
                          const uint64_t* content, size_t component,
                          size_t microslice);

    size_t _timeslice_count = 0;
    size_t _microslice_count = 0;
    size_t _content_bytes = 0;

    uint8_t _frame_number = 0;
    uint16_t _pgen_sequence_number = 0;
};
