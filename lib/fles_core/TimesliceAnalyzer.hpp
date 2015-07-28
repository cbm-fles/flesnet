// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Timeslice.hpp"
#include "MicrosliceDescriptor.hpp"
#include "interface.h" // crcutil_interface

class TimesliceAnalyzer
{
public:
    TimesliceAnalyzer();
    ~TimesliceAnalyzer();

    bool check_timeslice(const fles::Timeslice& ts);

    std::string statistics() const;
    void reset()
    {
        _microslice_count = 0;
        _content_bytes = 0;
    }

    size_t count() const { return _timeslice_count; }

private:
    uint32_t compute_crc(const fles::MicrosliceView m) const;

    bool check_crc(const fles::MicrosliceView m) const;

    bool check_flesnet_pattern(const fles::MicrosliceView m, size_t component);

    bool check_content_pgen(const uint16_t* content, size_t size);

    bool check_cbmnet_frames(const uint16_t* content, size_t size,
                             uint8_t sys_id, uint8_t sys_ver);

    bool check_flib_legacy_pattern(const fles::MicrosliceView m,
                                   size_t /* component */);

    bool check_flib_pattern(const fles::MicrosliceView m);

    bool check_microslice(const fles::MicrosliceView m, size_t component,
                          size_t microslice);

    crcutil_interface::CRC* _crc32_engine = nullptr;

    size_t _timeslice_count = 0;
    size_t _microslice_count = 0;
    size_t _content_bytes = 0;

    uint8_t _frame_number = 0;
    uint16_t _pgen_sequence_number = 0;
    uint32_t _flib_pgen_packet_number = 0;
};
