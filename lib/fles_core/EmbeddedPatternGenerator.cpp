// Copyright 2012-2014 Jan de Cuveland <cmail@cuveland.de>

#include "EmbeddedPatternGenerator.hpp"

void EmbeddedPatternGenerator::proceed()
{
    const DualRingBufferIndex min_avail = {desc_buffer_.size() / 4,
                                           data_buffer_.size() / 4};

    // break unless significant space is available
    if ((write_index_.data - read_index_.data + min_avail.data >
         data_buffer_.size()) ||
        (write_index_.desc - read_index_.desc + min_avail.desc >
         desc_buffer_.size())) {
        return;
    }

    while (true) {
        unsigned int content_bytes = typical_content_size_;
        if (randomize_sizes_)
            content_bytes = random_distribution_(random_generator_);
        content_bytes &= ~0x7u; // round down to multiple of sizeof(uint64_t)

        // check for space in data and descriptor buffers
        if ((write_index_.data - read_index_.data + content_bytes >
             data_buffer_.bytes()) ||
            (write_index_.desc - read_index_.desc + 1 > desc_buffer_.size()))
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
            generate_pattern_ ? fles::SubsystemFormatFLES::BasicRampPattern
                              : fles::SubsystemFormatFLES::Uninitialized);
        uint64_t idx = write_index_.desc;
        uint32_t crc = 0x00000000;
        uint32_t size = content_bytes;
        uint64_t offset = write_index_.data;

        // write to data buffer
        if (generate_pattern_) {
            for (uint64_t i = 0; i < content_bytes; i += sizeof(uint64_t)) {
                uint64_t data_word = (input_index_ << 48L) | i;
                reinterpret_cast<volatile uint64_t&>(
                    data_buffer_.at(write_index_.data)) = data_word;
                write_index_.data += sizeof(uint64_t);
                crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
            }
        } else {
            write_index_.data += content_bytes;
        }

        // write to descriptor buffer
        const_cast<fles::MicrosliceDescriptor&>(
            desc_buffer_.at(write_index_.desc++)) =
            fles::MicrosliceDescriptor({hdr_id, hdr_ver, eq_id, flags, sys_id,
                                        sys_ver, idx, crc, size, offset});
    }
}
