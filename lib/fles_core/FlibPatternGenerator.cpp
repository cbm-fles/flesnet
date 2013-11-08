// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "FlibPatternGenerator.hpp"

/// The thread main function.
void FlibPatternGenerator::produce_data()
{
    try
    {
        set_cpu(3);

        /// A pseudo-random number generator.
        std::default_random_engine random_generator;

        /// Distribution to use in determining data content sizes.
        std::poisson_distribution<> random_distribution(_typical_content_size);

        uint64_t written_mc = 0;
        uint64_t written_data = 0;

        uint64_t last_written_mc = 0;
        uint64_t last_written_data = 0;

        uint64_t acked_mc = 0;
        uint64_t acked_data = 0;

        const uint64_t min_avail_mc = _desc_buffer.size() / 4;
        const uint64_t min_avail_data = _data_buffer.bytes() / 4;

        const uint64_t min_written_mc = _desc_buffer.size() / 4;
        const uint64_t min_written_data = _data_buffer.bytes() / 4;

        while (true) {
            // wait until significant space is available
            last_written_mc = written_mc;
            last_written_data = written_data;
            {
                ++DCOUNT[0];
                std::unique_lock<std::mutex> l(_mutex);
                _written_mc = written_mc;
                _written_data = written_data;
                if (_is_stopped)
                    return;
                while ((written_data - _acked_data + min_avail_data
                        > _data_buffer.bytes())
                       || (written_mc - _acked_mc + min_avail_mc
                           > _desc_buffer.size())) {
                    ++DCOUNT[1];
                    _cond_producer.wait(l);
                    if (_is_stopped)
                        return;
                }
                acked_mc = _acked_mc;
                acked_data = _acked_data;
            }

            while (true) {
                unsigned int content_bytes = _typical_content_size;
                if (_randomize_sizes)
                    content_bytes = random_distribution(random_generator);

                // check for space in data and descriptor buffers
                if ((written_data - acked_data + content_bytes
                     > _data_buffer.bytes())
                    || (written_mc - acked_mc + 1 > _desc_buffer.size()))
                    break;

                const uint8_t hdr_id = 0xdd;
                const uint8_t hdr_ver = 0x01;
                const uint16_t eq_id = 0x1001;
                const uint16_t flags = 0x0000;
                const uint8_t sys_id = 0x01;
                const uint8_t sys_ver = 0x01;
                uint64_t idx = written_mc;
                uint32_t crc = 0x00000000;
                uint32_t size = content_bytes;
                uint64_t offset = written_data;

                // write to data buffer
                if (_generate_pattern) {
                    for (uint64_t i = 0; i < content_bytes;
                         i += sizeof(uint64_t)) {
                        uint64_t data_word = (_input_index << 48L) | i;
                        reinterpret_cast
                            <uint64_t&>(_data_buffer.at(written_data))
                            = data_word;
                        written_data += sizeof(uint64_t);
                        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
                    }
                } else {
                    written_data += content_bytes;
                }

                // write to descriptor buffer
                _desc_buffer.at(written_mc++) = MicrosliceDescriptor(
                    {hdr_id,  hdr_ver, eq_id, flags, sys_id,
                     sys_ver, idx,     crc,   size,  offset});

                if (written_mc >= last_written_mc + min_written_mc
                    || written_data >= last_written_data + min_written_data) {
                    last_written_mc = written_mc;
                    last_written_data = written_data;
                    {
                        ++DCOUNT[2];
                        std::unique_lock<std::mutex> l(_mutex);
                        _written_mc = written_mc;
                        _written_data = written_data;
                    }
                    _cond_consumer.notify_one();
                }
            }
        }
    }
    catch (std::exception& e)
    {
        out.error() << "exception in FlibPatternGenerator: " << e.what();
    }
}
