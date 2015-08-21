// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

// Info for FLIB pattern checking:
// Timeslices are treated as independet
// Microslices are treated as consecutive data stream
// Checks for:
// - consecutive CBMnet frame nubers across all microclices
//   in one timeslice component
// - cosecutive pgen sequence numbers across all microslices
//   in one timeslice component
// Implementation is not dump parallelizable across ts components!

#include "TimesliceAnalyzer.hpp"
#include <iostream>
#include <sstream>
#include <cassert>

TimesliceAnalyzer::TimesliceAnalyzer(uint64_t arg_output_interval,
                                     std::ostream& arg_out,
                                     std::string arg_output_prefix)
    : output_interval_(arg_output_interval), out_(arg_out),
      output_prefix_(arg_output_prefix)
{
    // create CRC-32C engine (Castagnoli polynomial)
    crc32_engine_ = crcutil_interface::CRC::Create(
        0x82f63b78, 0, 32, true, 0, 0, 0,
        crcutil_interface::CRC::IsSSE42Available(), NULL);
}

TimesliceAnalyzer::~TimesliceAnalyzer()
{
    if (crc32_engine_) {
        crc32_engine_->Delete();
    }
}

uint32_t TimesliceAnalyzer::compute_crc(const fles::MicrosliceView m) const
{
    assert(crc32_engine_);

    crcutil_interface::UINT64 crc64 = 0;
    crc32_engine_->Compute(m.content(), m.desc().size, &crc64);

    return static_cast<uint32_t>(crc64);
}

bool TimesliceAnalyzer::check_crc(const fles::MicrosliceView m) const
{
    return compute_crc(m) == m.desc().crc;
}

bool TimesliceAnalyzer::check_flesnet_pattern(const fles::MicrosliceView m,
                                              size_t component)
{
    const uint64_t* content = reinterpret_cast<const uint64_t*>(m.content());
    uint32_t crc = 0x00000000;
    for (size_t pos = 0; pos < m.desc().size / sizeof(uint64_t); ++pos) {
        uint64_t data_word = content[pos];
        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32);
        uint64_t expected =
            (static_cast<uint64_t>(component) << 48) | (pos * sizeof(uint64_t));
        if (data_word != expected)
            return false;
    }
    if (crc != m.desc().crc)
        return false;
    return true;
}

bool TimesliceAnalyzer::check_content_pgen(const uint16_t* content, size_t size)
{
    constexpr uint16_t source_address = 0;

    if (content[0] != source_address) {
        std::cerr << "unexpected source address: " << content[0] << std::endl;
        return false;
    }

    for (size_t i = 1; i < size - 1; ++i) {
        uint8_t low = content[i] & 0xff;
        uint8_t high = (content[i] >> 8) & 0xff;
        if (high != 0xbc || low != i - 1) {
            std::cerr << "unexpected cbmnet content word: " << content[i]
                      << std::endl;
            return false;
        }
    }

    uint16_t pgen_sequence_number = content[size - 1];
    uint16_t expected_pgen_sequence_number = pgen_sequence_number_ + 1;
    // uncomment to check only for increasing sequence numbers
    //    if (pgen_sequence_number_ != 0 && pgen_sequence_number != 0 &&
    //        pgen_sequence_number < expected_pgen_sequence_number) {
    if (pgen_sequence_number_ != 0 &&
        pgen_sequence_number != expected_pgen_sequence_number) {
        std::cerr << "unexpected pgen sequence number in frame "
                  << static_cast<unsigned>(frame_number_) << ":  expected "
                  << expected_pgen_sequence_number << "  found "
                  << pgen_sequence_number << std::endl;
        return false;
    }
    pgen_sequence_number_ = pgen_sequence_number;

    return true;
}

bool TimesliceAnalyzer::check_cbmnet_frames(const uint16_t* content,
                                            size_t size, uint8_t sys_id,
                                            uint8_t sys_ver)
{
    size_t i = 0;
    while (i < size) {
        uint8_t frame_number = (content[i] >> 8) & 0xff;
        uint8_t word_count =
            (content[i] & 0xff) + 1; // FIXME: Definition in hardware
        uint8_t padding_count = (4 - ((word_count + 1) & 0x3)) & 0x3;
        ++i;

        uint8_t expected_frame_number = frame_number_ + 1;
        if (frame_number_ != 0 && frame_number != expected_frame_number) {
            std::cerr << "unexpected cbmnet frame number:"
                      << "  expected: "
                      << static_cast<uint32_t>(expected_frame_number)
                      << "  found: " << static_cast<uint32_t>(frame_number)
                      << std::endl;
            return false;
        }
        frame_number_ = frame_number;

        if (word_count < 4 || word_count > 64 ||
            i + word_count + padding_count > size) {
            std::cerr << "invalid cbmnet frame word count: " << word_count
                      << std::endl;
            return false;
        }

        if (sys_id == static_cast<uint8_t>(fles::SubsystemIdentifier::FLES) &&
            sys_ver == static_cast<uint8_t>(
                           fles::SubsystemFormatFLES::CbmNetPattern)) {
            if (check_content_pgen(&content[i], word_count) == false)
                return false;
        }
        i += word_count + padding_count;
    }

    return true;
}

bool TimesliceAnalyzer::check_flib_legacy_pattern(const fles::MicrosliceView m,
                                                  size_t /* component */)
{
    const uint64_t* content = reinterpret_cast<const uint64_t*>(m.content());
    if (content[0] != reinterpret_cast<const uint64_t*>(&m.desc())[0] ||
        content[1] != reinterpret_cast<const uint64_t*>(&m.desc())[1]) {
        return false;
    }
    return check_cbmnet_frames(reinterpret_cast<const uint16_t*>(&content[2]),
                               (m.desc().size - 16) / sizeof(uint16_t),
                               m.desc().sys_id, m.desc().sys_ver);
}

bool TimesliceAnalyzer::check_flib_pattern(const fles::MicrosliceView m)
{
    uint8_t last_word_size = 0;

    // increment packte number if initialized
    if (flib_pgen_packet_number_ != 0) {
        ++flib_pgen_packet_number_;
    }

    if (m.desc().size >= 1) {
        last_word_size = m.content()[0];
        if ((m.desc().size & 0x7) != (last_word_size & 0x7)) {
            std::cerr << "Flib pgen: error in last word size" << std::endl;
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

bool TimesliceAnalyzer::check_microslice(const fles::MicrosliceView m,
                                         size_t component, size_t microslice)
{
    if (m.desc().idx != microslice) {
        std::cerr << "microslice index " << m.desc().idx
                  << " found in m.desc() " << microslice << std::endl;
        return false;
    }

    ++microslice_count_;
    content_bytes_ += m.desc().size;

    if (m.desc().flags &
            static_cast<uint16_t>(fles::MicrosliceFlags::CrcValid) &&
        check_crc(m) == false) {
        std::cerr << "crc failure in microslice " << m.desc().idx << std::endl;
        return false;
    }

    if (static_cast<fles::SubsystemIdentifier>(m.desc().sys_id) !=
        fles::SubsystemIdentifier::FLES) {
        return true;
    }

    switch (static_cast<fles::SubsystemFormatFLES>(m.desc().sys_ver)) {
    case fles::SubsystemFormatFLES::BasicRampPattern:
        return check_flesnet_pattern(m, component);
    case fles::SubsystemFormatFLES::CbmNetPattern:
    case fles::SubsystemFormatFLES::CbmNetFrontendEmulation:
        return check_flib_legacy_pattern(m, component);
    case fles::SubsystemFormatFLES::FlibPattern:
        return check_flib_pattern(m);
    default:
        return true;
    }
    return true;
}

bool TimesliceAnalyzer::check_timeslice(const fles::Timeslice& ts)
{
    if (ts.num_components() == 0) {
        std::cerr << "no component in TS " << ts.index() << std::endl;
        return false;
    }

    ++timeslice_count_;
    for (size_t c = 0; c < ts.num_components(); ++c) {
        if (ts.num_microslices(c) == 0) {
            std::cerr << "no microslices in TS " << ts.index() << ", component "
                      << c << std::endl;
            return false;
        }
        frame_number_ = 0; // reset frame number for next component
        pgen_sequence_number_ = 0;
        flib_pgen_packet_number_ = 0;
        for (size_t m = 0; m < ts.num_microslices(c); ++m) {
            bool success =
                check_microslice(ts.get_microslice(c, m), c,
                                 ts.index() * ts.num_core_microslices() + m);
            if (!success) {
                std::cerr << "pattern error in TS " << ts.index() << ", MC "
                          << m << ", component " << c << std::endl;
                return false;
            }
        }
    }
    return true;
}

std::string TimesliceAnalyzer::statistics() const
{
    std::stringstream s;
    s << "timeslices checked: " << timeslice_count_ << " (" << content_bytes_
      << " bytes in " << microslice_count_ << " microslices, avg: "
      << static_cast<double>(content_bytes_) / microslice_count_ << ")";
    return s.str();
}

void TimesliceAnalyzer::put(const fles::Timeslice& timeslice)
{
    check_timeslice(timeslice);
    if ((count() % 10000) == 0) {
        out_ << output_prefix_ << statistics() << std::endl;
        reset();
    }
}
