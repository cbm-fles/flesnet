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

        DualRingBufferIndex write_index = {0, 0};
        DualRingBufferIndex last_write_index = {0, 0};
        DualRingBufferIndex read_index = {0, 0};

        const DualRingBufferIndex min_avail = {desc_buffer_.size() / 4,
                                               data_buffer_.size() / 4};

        const DualRingBufferIndex min_written = {desc_buffer_.size() / 4,
                                                 data_buffer_.size() / 4};

        while (true) {
            // wait until significant space is available
            last_write_index = write_index;
            write_index_.store(write_index);

            do {
                if (is_stopped_)
                    return;
                read_index = read_index_.load();
            } while ((write_index.data - read_index.data + min_avail.data >
                      data_buffer_.size()) ||
                     (write_index.desc - read_index.desc + min_avail.desc >
                      desc_buffer_.size()));

            while (true) {
                unsigned int content_bytes = typical_content_size_;
                if (randomize_sizes_)
                    content_bytes = random_distribution(random_generator);
                content_bytes &=
                    ~0x7u; // round down to multiple of sizeof(uint64_t)

                // check for space in data and descriptor buffers
                if ((write_index.data - read_index.data + content_bytes >
                     data_buffer_.bytes()) ||
                    (write_index.desc - read_index.desc + 1 >
                     desc_buffer_.size()))
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
                uint64_t idx = write_index.desc;
                uint32_t crc = 0x00000000;
                uint32_t size = content_bytes;
                uint64_t offset = write_index.data;

                // write to data buffer
                if (generate_pattern_) {
                    for (uint64_t i = 0; i < content_bytes;
                         i += sizeof(uint64_t)) {
                        uint64_t data_word = (input_index_ << 48L) | i;
                        reinterpret_cast<volatile uint64_t&>(
                            data_buffer_.at(write_index.data)) = data_word;
                        write_index.data += sizeof(uint64_t);
                        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
                    }
                } else {
                    write_index.data += content_bytes;
                }

                // write to descriptor buffer
                const_cast<fles::MicrosliceDescriptor&>(
                    desc_buffer_.at(write_index.desc++)) =
                    fles::MicrosliceDescriptor({hdr_id, hdr_ver, eq_id, flags,
                                                sys_id, sys_ver, idx, crc, size,
                                                offset});

                if (write_index.desc >=
                        last_write_index.desc + min_written.desc ||
                    write_index.data >=
                        last_write_index.data + min_written.data) {
                    last_write_index = write_index;
                    write_index_.store(write_index);
                }
            }
        }
    } catch (std::exception& e) {
        L_(error) << "exception in FlibPatternGenerator: " << e.what();
    }
}
