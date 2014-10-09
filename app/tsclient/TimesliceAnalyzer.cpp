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

bool TimesliceAnalyzer::check_flesnet_pattern(
    const fles::MicrosliceDescriptor& descriptor, const uint64_t* content,
    size_t component)
{
    uint32_t crc = 0x00000000;
    for (size_t pos = 0; pos < descriptor.size / sizeof(uint64_t); ++pos) {
        uint64_t data_word = content[pos];
        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32);
        uint64_t expected =
            (static_cast<uint64_t>(component) << 48) | (pos * sizeof(uint64_t));
        if (data_word != expected)
            return false;
    }
    if (crc != descriptor.crc)
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
    uint16_t expected_pgen_sequence_number = _pgen_sequence_number + 1;
    // uncomment to check only for increasing sequence numbers
    //    if (_pgen_sequence_number != 0 && pgen_sequence_number != 0 &&
    //        pgen_sequence_number < expected_pgen_sequence_number) {
    if (_pgen_sequence_number != 0 &&
        pgen_sequence_number != expected_pgen_sequence_number) {
        std::cerr << "unexpected pgen sequence number in frame "
                  << static_cast<unsigned>(_frame_number) << ":  expected "
                  << expected_pgen_sequence_number << "  found "
                  << pgen_sequence_number << std::endl;
        return false;
    }
    _pgen_sequence_number = pgen_sequence_number;

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

        uint8_t expected_frame_number = _frame_number + 1;
        if (_frame_number != 0 && frame_number != expected_frame_number) {
            std::cerr << "unexpected cbmnet frame number:"
                      << "  expected: "
                      << static_cast<uint32_t>(expected_frame_number)
                      << "  found: " << static_cast<uint32_t>(frame_number)
                      << std::endl;
            return false;
        }
        _frame_number = frame_number;

        if (word_count < 4 || word_count > 64 ||
            i + word_count + padding_count > size) {
            std::cerr << "invalid cbmnet frame word count: " << word_count
                      << std::endl;
            return false;
        }

        if (sys_id == static_cast<uint8_t>(0xF0) &&
            sys_ver == static_cast<uint8_t>(0x1)) {
            if (check_content_pgen(&content[i], word_count) == false)
                return false;
        }
        i += word_count + padding_count;
    }

    return true;
}

bool TimesliceAnalyzer::check_flib_pattern(
    const fles::MicrosliceDescriptor& descriptor, const uint64_t* content,
    size_t /* component */)
{
    if (content[0] != reinterpret_cast<const uint64_t*>(&descriptor)[0] ||
        content[1] != reinterpret_cast<const uint64_t*>(&descriptor)[1]) {
        return false;
    }
    return check_cbmnet_frames(reinterpret_cast<const uint16_t*>(&content[2]),
                               (descriptor.size - 16) / sizeof(uint16_t),
                               descriptor.sys_id, descriptor.sys_ver);
}

bool TimesliceAnalyzer::check_microslice(
    const fles::MicrosliceDescriptor& descriptor, const uint64_t* content,
    size_t component, size_t microslice)
{
    if (descriptor.idx != microslice) {
        std::cerr << "microslice index " << descriptor.idx
                  << " found in descriptor " << microslice << std::endl;
        return false;
    }

    ++_microslice_count;
    _content_bytes += descriptor.size;

    switch (descriptor.sys_id) {
    case 0xFA:
        return check_flesnet_pattern(descriptor, content, component);
    default:
        return check_flib_pattern(descriptor, content, component);
    }
    return true;
}

bool TimesliceAnalyzer::check_timeslice(const fles::Timeslice& ts)
{
    if (ts.num_components() == 0) {
        std::cerr << "no component in TS " << ts.index() << std::endl;
        return false;
    }

    ++_timeslice_count;
    for (size_t c = 0; c < ts.num_components(); ++c) {
        if (ts.num_microslices(c) == 0) {
            std::cerr << "no microslices in TS " << ts.index() << ", component "
                      << c << std::endl;
            return false;
        }
        _frame_number = 0; // reset frame number for next component
        _pgen_sequence_number = 0;
        for (size_t m = 0; m < ts.num_microslices(c); ++m) {
            bool success = check_microslice(
                ts.descriptor(c, m),
                reinterpret_cast<const uint64_t*>(ts.content(c, m)), c,
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
    s << "timeslices checked: " << _timeslice_count << " (" << _content_bytes
      << " bytes in " << _microslice_count << " microslices, avg: "
      << static_cast<double>(_content_bytes) / _microslice_count << ")";
    return s.str();
}
