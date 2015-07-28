// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "FlibPatternGenerator.hpp"

/// The thread main function.
void FlibPatternGenerator::produce_data()
{
    try {
        set_cpu(3);

        /// A pseudo-random number generator.
        std::default_random_engine random_generator;

        /// Distribution to use in determining data content sizes.
        std::poisson_distribution<unsigned int> random_distribution(
            typical_content_size_);

        uint64_t written_desc = 0;
        uint64_t written_data = 0;

        uint64_t last_written_desc = 0;
        uint64_t last_written_data = 0;

        uint64_t acked_desc = 0;
        uint64_t acked_data = 0;

        const uint64_t min_avail_desc = desc_buffer_.size() / 4;
        const uint64_t min_avail_data = data_buffer_.bytes() / 4;

        const uint64_t min_written_desc = desc_buffer_.size() / 4;
        const uint64_t min_written_data = data_buffer_.bytes() / 4;

        while (true) {
            // wait until significant space is available
            last_written_desc = written_desc;
            last_written_data = written_data;
            written_desc_ = written_desc;
            written_data_ = written_data;
            if (is_stopped_)
                return;
            while ((written_data - acked_data_ + min_avail_data >
                    data_buffer_.bytes()) ||
                   (written_desc - acked_desc_ + min_avail_desc >
                    desc_buffer_.size())) {
                if (is_stopped_)
                    return;
            }
            acked_desc = acked_desc_;
            acked_data = acked_data_;

            while (true) {
                unsigned int content_bytes = typical_content_size_;
                if (randomize_sizes_)
                    content_bytes = random_distribution(random_generator);
                content_bytes &=
                    ~0x7u; // round down to multiple of sizeof(uint64_t)

                // check for space in data and descriptor buffers
                if ((written_data - acked_data + content_bytes >
                     data_buffer_.bytes()) ||
                    (written_desc - acked_desc + 1 > desc_buffer_.size()))
                    break;

                const uint8_t hdr_id = static_cast<uint8_t>(
                    fles::HeaderFormatIdentifier::Standard);
                const uint8_t hdr_ver =
                    static_cast<uint8_t>(fles::HeaderFormatVersion::Standard);
                const uint16_t eq_id = 0xE001;
                const uint16_t flags = 0x0000;
                const uint8_t sys_id =
                    static_cast<uint8_t>(fles::SubsystemIdentifier::FLES);
                const uint8_t sys_ver = static_cast<uint8_t>(
                    generate_pattern_
                        ? fles::SubsystemFormatFLES::BasicRampPattern
                        : fles::SubsystemFormatFLES::Uninitialized);
                uint64_t idx = written_desc;
                uint32_t crc = 0x00000000;
                uint32_t size = content_bytes;
                uint64_t offset = written_data;

                // write to data buffer
                if (generate_pattern_) {
                    for (uint64_t i = 0; i < content_bytes;
                         i += sizeof(uint64_t)) {
                        uint64_t data_word = (input_index_ << 48L) | i;
                        reinterpret_cast<volatile uint64_t&>(
                            data_buffer_.at(written_data)) = data_word;
                        written_data += sizeof(uint64_t);
                        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
                    }
                } else {
                    written_data += content_bytes;
                }

                // write to descriptor buffer
                const_cast<fles::MicrosliceDescriptor&>(
                    desc_buffer_.at(written_desc++)) =
                    fles::MicrosliceDescriptor({hdr_id, hdr_ver, eq_id, flags,
                                                sys_id, sys_ver, idx, crc, size,
                                                offset});

                if (written_desc >= last_written_desc + min_written_desc ||
                    written_data >= last_written_data + min_written_data) {
                    last_written_desc = written_desc;
                    last_written_data = written_data;
                    written_desc_ = written_desc;
                    written_data_ = written_data;
                }
            }
        }
    } catch (std::exception& e) {
        L_(error) << "exception in FlibPatternGenerator: " << e.what();
    }
}
