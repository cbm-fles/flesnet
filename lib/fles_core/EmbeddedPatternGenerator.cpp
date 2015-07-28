// Copyright 2012-2014 Jan de Cuveland <cmail@cuveland.de>

#include "EmbeddedPatternGenerator.hpp"

void EmbeddedPatternGenerator::proceed()
{
    const uint64_t min_avail_desc = _desc_buffer.size() / 4;
    const uint64_t min_avail_data = _data_buffer.bytes() / 4;

    // break unless significant space is available
    if ((_written_data - _acked_data + min_avail_data > _data_buffer.bytes()) ||
        (_written_desc - _acked_desc + min_avail_desc > _desc_buffer.size())) {
        return;
    }

    while (true) {
        unsigned int content_bytes = _typical_content_size;
        if (_randomize_sizes)
            content_bytes = _random_distribution(_random_generator);
        content_bytes &= ~0x7u; // round down to multiple of sizeof(uint64_t)

        // check for space in data and descriptor buffers
        if ((_written_data - _acked_data + content_bytes >
             _data_buffer.bytes()) ||
            (_written_desc - _acked_desc + 1 > _desc_buffer.size()))
            return;

        const uint8_t hdr_id =
            static_cast<uint8_t>(fles::HeaderFormatIdentifier::Standard);
        const uint8_t hdr_ver =
            static_cast<uint8_t>(fles::HeaderFormatVersion::Standard);
        const uint16_t eq_id = 0xE001;
        const uint16_t flags = 0x0000;
        const uint8_t sys_id =
            static_cast<uint8_t>(fles::SubsystemIdentifier::FLES);
        const uint8_t sys_ver = static_cast<uint8_t>(
            _generate_pattern ? fles::SubsystemFormatFLES::BasicRampPattern
                              : fles::SubsystemFormatFLES::Uninitialized);
        uint64_t idx = _written_desc;
        uint32_t crc = 0x00000000;
        uint32_t size = content_bytes;
        uint64_t offset = _written_data;

        // write to data buffer
        if (_generate_pattern) {
            for (uint64_t i = 0; i < content_bytes; i += sizeof(uint64_t)) {
                uint64_t data_word = (_input_index << 48L) | i;
                reinterpret_cast<volatile uint64_t&>(
                    _data_buffer.at(_written_data)) = data_word;
                _written_data += sizeof(uint64_t);
                crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
            }
        } else {
            _written_data += content_bytes;
        }

        // write to descriptor buffer
        const_cast<fles::MicrosliceDescriptor&>(
            _desc_buffer.at(_written_desc++)) =
            fles::MicrosliceDescriptor({hdr_id, hdr_ver, eq_id, flags, sys_id,
                                        sys_ver, idx, crc, size, offset});
    }
}
