// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>

// Info for FLIB pattern checking:
// Timeslices are treated as independet
// Microslices are treated as consecutive data stream
// Checks for:
// - consecutive CBMnet frame nubers across all microclices
//   in one timeslice component
// - cosecutive pgen sequence numbers across all microslices
//   in one timeslice component
// Implementation is not dump parallelizable across ts components!

#include "FlibPatternChecker.hpp"
#include <iostream>

bool FlibPatternChecker::check(const fles::Microslice& m)
{
    uint8_t last_word_size = 0;

    // increment packte number if initialized
    if (flib_pgen_packet_number_ != 0) {
        ++flib_pgen_packet_number_;
    }

    // Do not check last word size and last word if ms was truncated
    // Last word check will be skipped because last_word_size = 0
    if (m.desc().size >= 1 &&
        ((m.desc().flags &
          static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) == 0)) {
        last_word_size = m.content()[0];
        if ((m.desc().size & 0x7) != (last_word_size & 0x7)) {
            std::cerr << "Flib pgen: error in last word size" << std::endl;
            std::cerr << "desc.size " << m.desc().size << std::endl;
            std::cerr << "last word " << static_cast<uint32_t>(last_word_size)
                      << std::endl;
            return false;
        }
    }

    if (m.desc().size >= 4) {
        const uint16_t word = reinterpret_cast<const uint16_t*>(m.content())[1];
        if (word != 0xBBFF) {
            std::cerr << "Flib pgen: error in hdr word" << std::endl;
            return false;
        }
    }

    if (m.desc().size >= 8) {
        uint32_t flib_pgen_packet_number =
            reinterpret_cast<const uint32_t*>(m.content())[1];
        // check if initalized
        if (flib_pgen_packet_number_ != 0 &&
            flib_pgen_packet_number_ != flib_pgen_packet_number) {
            std::cerr << "Flib pgen: error in packet number" << std::endl;
            return false;
        }
        // initialize if uninitialized
        if (flib_pgen_packet_number_ == 0) {
            flib_pgen_packet_number_ = flib_pgen_packet_number;
        }
    }

    // check ramp, everything form second word but last word
    // last word is ramp word if last_word_size is 0
    if (m.desc().size > 8) {
        size_t ramp_limit = 0;
        if (last_word_size == 0) {
            ramp_limit = 8;
        } else {
            ramp_limit = 9;
        }
        size_t pos = 1;
        uint64_t ramp = 0xABCD000000000000;
        const uint64_t* content =
            reinterpret_cast<const uint64_t*>(m.content()) + 0;

        while (pos <= ((m.desc().size - ramp_limit) / sizeof(uint64_t))) {
            if (content[pos] != ramp) {
                std::cerr << "Flib pgen: error in ramp word "
                          << " exp " << std::hex << ramp << " seen "
                          << content[pos] << std::endl;
                return false;
            }
            ++ramp;
            ++pos;
        }

        // check last word if any
        size_t last_word_start = pos * sizeof(uint64_t);
        for (size_t i = 0; i < last_word_size; ++i) {
            if (m.content()[last_word_start + i] != 0xFA) {
                std::cerr << "Flib pgen: error in last word" << std::endl;
                return false;
            }
        }
    }

    return true;
}
