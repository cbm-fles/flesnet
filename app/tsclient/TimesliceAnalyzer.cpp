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

bool TimesliceAnalyzer::check_flib_pattern(
                                           const fles::MicrosliceDescriptor& /* descriptor */,
                                           const uint64_t* /* content */,
                                           size_t /* component */)
{
    // TODO
    return true;
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
