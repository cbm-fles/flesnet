#pragma once
/**
 * \file FlibPatternGenerator.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

/// Simple software pattern generator used as FLIB replacement.
class FlibPatternGenerator : public DataSource, public ThreadContainer
{
public:
    /// The FlibPatternGenerator constructor.
    FlibPatternGenerator(std::size_t data_buffer_size_exp,
              std::size_t desc_buffer_size_exp,
              uint64_t input_index,
              bool generate_pattern,
              uint32_t typical_content_size,
              bool randomize_sizes) :
        _data_buffer(data_buffer_size_exp),
        _desc_buffer(desc_buffer_size_exp),
        _data_buffer_view(_data_buffer.ptr(), data_buffer_size_exp),
        _desc_buffer_view(_desc_buffer.ptr(), desc_buffer_size_exp),
        _input_index(input_index),
        _generate_pattern(generate_pattern),
        _typical_content_size(typical_content_size),
        _randomize_sizes(randomize_sizes)
    {
        _producer_thread = new std::thread(&FlibPatternGenerator::produce_data, this);
    }

    FlibPatternGenerator(const FlibPatternGenerator&) = delete;
    void operator=(const FlibPatternGenerator&) = delete;

    ~FlibPatternGenerator()
    {
        {
            std::unique_lock<std::mutex> l(_mutex);
            _is_stopped = true;
        }
        _cond_producer.notify_one();
        _producer_thread->join();
        delete _producer_thread;


        for (int i = 0; i < 10; ++i)
            out.trace() << "DCOUNT[" << i << "] = " << DCOUNT[i];
    }

    virtual RingBufferView<>& data_buffer()
    {
        return _data_buffer_view;
    }

    virtual RingBufferView<MicrosliceDescriptor>& desc_buffer()
    {
        return _desc_buffer_view;
    }

    /// Generate FLIB input data.
    void produce_data()
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
                while ((written_data - _acked_data + min_avail_data > _data_buffer.bytes())
                       || (written_mc - _acked_mc + min_avail_mc > _desc_buffer.size())) {
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
                if ((written_data - acked_data + content_bytes > _data_buffer.bytes())
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
                    for (uint64_t i = 0; i < content_bytes; i+= sizeof(uint64_t)) {
                        uint64_t data_word = (_input_index << 48L) | i;
                        reinterpret_cast<uint64_t&>(_data_buffer.at(written_data)) = data_word;
                        written_data += sizeof(uint64_t);
                        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
                    }
                } else {
                    written_data += content_bytes;
                }

                // write to descriptor buffer
                _desc_buffer.at(written_mc++) = MicrosliceDescriptor({hdr_id, hdr_ver,
                            eq_id, flags, sys_id, sys_ver, idx, crc, size, offset});

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

    virtual uint64_t wait_for_data(uint64_t min_mc_number)
    {
++DCOUNT[3];
        std::unique_lock<std::mutex> l(_mutex);
        while (min_mc_number > _written_mc) {
++DCOUNT[4];
            _cond_consumer.wait(l);
        }
        return _written_mc;
    }

    virtual void update_ack_pointers(uint64_t new_acked_data, uint64_t new_acked_mc)
    {
++DCOUNT[6];
        {
            std::unique_lock<std::mutex> l(_mutex);
            _acked_data = new_acked_data;
            _acked_mc = new_acked_mc;
        }
        _cond_producer.notify_one();
    }

private:
    uint64_t DCOUNT[10] = {};

    /// Input data buffer.
    RingBuffer<> _data_buffer;

    /// Input descriptor buffer.
    RingBuffer<MicrosliceDescriptor> _desc_buffer;

    RingBufferView<> _data_buffer_view;
    RingBufferView<MicrosliceDescriptor> _desc_buffer_view;

    /// This node's index in the list of input nodes
    uint64_t _input_index;

    bool _generate_pattern;
    uint32_t _typical_content_size;
    bool _randomize_sizes;

    std::atomic<bool> _is_stopped{false};

    std::thread* _producer_thread;

    std::mutex _mutex;
    std::condition_variable _cond_producer;
    std::condition_variable _cond_consumer;

    /// Number of acknowledged data bytes. Updated by input node.
    uint64_t _acked_data{0};
    
    /// Number of acknowledged MCs. Updated by input node.
    uint64_t _acked_mc{0};
    
    /// FLIB-internal number of written data bytes.
    uint64_t _written_data{0};

    /// FLIB-internal number of written MCs. 
    uint64_t _written_mc{0};
};
