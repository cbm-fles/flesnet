/**
 * \file DataSource.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef DATASOURCE_HPP
#define DATASOURCE_HPP

/// Abstract FLES data source class.
class DataSource
{
public:
    /// The DataSource constructor.
    DataSource(RingBuffer<>& data_buffer,
               RingBuffer<MicrosliceDescriptor>& desc_buffer) :
        _data_buffer(data_buffer),
        _desc_buffer(desc_buffer) { };
    
    virtual uint64_t wait_for_data(uint64_t min_mcNumber) = 0;

    virtual void update_ack_pointers(uint64_t new_acked_data, uint64_t new_acked_mc) = 0;
    
protected:
    /// Input data buffer.
    RingBuffer<>& _data_buffer;

    /// Input descriptor buffer.
    RingBuffer<MicrosliceDescriptor>& _desc_buffer;
};


/// Simple software pattern generator used as FLIB replacement.
class DummyFlib : public DataSource, public ThreadContainer
{
public:
    /// The DummyFlib constructor.
    DummyFlib(RingBuffer<>& data_buffer,
              RingBuffer<MicrosliceDescriptor>& desc_buffer) :
        DataSource(data_buffer, desc_buffer),
        _pd(par->typical_content_size()),
        _rand_content_bytes(_rng, _pd)
    {
        _producer_thread = new boost::thread(&DummyFlib::produce_data, this);
    };

    ~DummyFlib()
    {
        {
            boost::unique_lock<boost::mutex> l(_mutex);
            _is_stopped = true;
        }
        _cond_producer.notify_one();
        _producer_thread->join();
        delete _producer_thread;


        for (int i = 0; i < 10; i++)
            out.trace() << "DCOUNT[" << i << "] = " << DCOUNT[i];
    };

    uint64_t DCOUNT[10] = {};

    /// Generate FLIB input data.
    void produce_data()
    {
        set_cpu(3);

        bool generate_pattern = par->check_pattern();

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
DCOUNT[0]++;
                boost::unique_lock<boost::mutex> l(_mutex);
                _written_mc = written_mc;
                _written_data = written_data;
                if (_is_stopped)
                    return;
                while ((written_data - _acked_data + min_avail_data > _data_buffer.bytes())
                       || (written_mc - _acked_mc + min_avail_mc > _desc_buffer.size())) {
DCOUNT[1]++;
                    _cond_producer.wait(l);
                    if (_is_stopped)
                        return;
                }
                acked_mc = _acked_mc;
                acked_data = _acked_data;
            }

            while (true) {
                unsigned int content_bytes = par->typical_content_size();
                if (par->randomize_sizes())
                    content_bytes = _rand_content_bytes();

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
                if (generate_pattern) {
                    for (uint64_t i = 0; i < content_bytes; i+= sizeof(uint64_t)) {
                        uint64_t data_word = ((uint64_t) par->node_index() << 48) | i;
                        (uint64_t&) _data_buffer.at(written_data) = data_word;
                        written_data += sizeof(uint64_t);
                        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32);
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
DCOUNT[2]++;
                        boost::unique_lock<boost::mutex> l(_mutex);
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
        static uint64_t written_mc = 0;

DCOUNT[3]++;
        if (min_mc_number > written_mc) {
            boost::unique_lock<boost::mutex> l(_mutex);
            while (min_mc_number > _written_mc) {
DCOUNT[4]++;
                _cond_consumer.wait(l);
            }
            written_mc = _written_mc;
        }
        return written_mc;
    };

    virtual void update_ack_pointers(uint64_t new_acked_data, uint64_t new_acked_mc)
    {
        static uint64_t acked_mc = 0;
        static uint64_t acked_data = 0;

        const uint64_t min_acked_mc = _desc_buffer.size() / 4;
        const uint64_t min_acked_data = _data_buffer.size() / 4;

DCOUNT[5]++;
        if (new_acked_data >= acked_data + min_acked_data
            || new_acked_mc >= acked_mc + min_acked_mc) {
            acked_data = new_acked_data;
            acked_mc = new_acked_mc;
DCOUNT[6]++;
            {
                boost::unique_lock<boost::mutex> l(_mutex);
                _acked_data = acked_data;
                _acked_mc = acked_mc;
            }
            _cond_producer.notify_one();
        }
    };

private:
    std::atomic<bool> _is_stopped{false};

    boost::thread* _producer_thread;

    boost::mutex _mutex;
    boost::condition_variable _cond_producer;
    boost::condition_variable _cond_consumer;

    /// Number of acknowledged data bytes. Updated by input node.
    uint64_t _acked_data{0};
    
    /// Number of acknowledged MCs. Updated by input node.
    uint64_t _acked_mc{0};
    
    /// FLIB-internal number of written data bytes.
    uint64_t _written_data{0};

    /// FLIB-internal number of written MCs. 
    uint64_t _written_mc{0};

    /// A pseudo-random number generator.
    boost::mt19937 _rng;

    /// Distribution to use in determining data content sizes.
    boost::poisson_distribution<> _pd;

    /// Generate random number of content bytes.
    boost::variate_generator<boost::mt19937&,
                             boost::poisson_distribution<> > _rand_content_bytes;
};
    

#endif /* DATASOURCE_HPP */
