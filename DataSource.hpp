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
    DataSource(RingBuffer<uint64_t>& data_buffer,
               RingBuffer<uint64_t>& addr_buffer) :
        _data_buffer(data_buffer),
        _addr_buffer(addr_buffer) { };
    
    virtual void wait_for_data(uint64_t min_mcNumber) = 0;

    virtual void update_ack_pointers(uint64_t acked_data, uint64_t acked_mc) = 0;
    
protected:
    /// Input data buffer.
    RingBuffer<uint64_t>& _data_buffer;

    /// Input address buffer.
    RingBuffer<uint64_t>& _addr_buffer;
};


/// Simple software pattern generator used as FLIB replacement.
class DummyFlib : DataSource
{
public:
    /// The DummyFlib constructor.
    DummyFlib(RingBuffer<uint64_t>& data_buffer,
              RingBuffer<uint64_t>& addr_buffer) :
        DataSource(data_buffer, addr_buffer),
        _pd(par->typical_content_size()),
        _rand_content_words(_rng, _pd)
    {
        _generator_thread = new boost::thread(&DummyFlib::generate_data, this);
    };

    ~DummyFlib()
    {
        _is_stopped = true;
        _generator_thread->join();
        delete _generator_thread;
    };
    
    /// Generate FLIB input data.
    void generate_data()
    {
        bool generate_pattern = par->check_pattern();

        while (true) {
            unsigned int content_words = par->typical_content_size();
            if (par->randomize_sizes())
                content_words = _rand_content_words();

            uint8_t hdrrev = 0x01;
            uint8_t sysid = 0x01;
            uint16_t flags = 0x0000;
            uint32_t size = (content_words + 2) * 8;
            uint16_t rsvd = 0x0000;
            uint64_t time = _mc_written;

            uint64_t hdr0 = (uint64_t) hdrrev << 56 | (uint64_t) sysid << 48
                            | (uint64_t) flags << 32 | (uint64_t) size;
            uint64_t hdr1 = (uint64_t) rsvd << 48 | (time & 0xFFFFFFFFFFFF);

            // check for space in data and addr buffers, wait if required
            {
                //                boost::unique_lock<boost::mutex> lock(_acked_mutex);
                while ((_data_written - _acked_data + content_words + 2 > _data_buffer.size())
                       || (_mc_written - _acked_mc + 1 > _addr_buffer.size())) {
                    //                    _cond_acked.wait(lock);
                    if (_is_stopped) return;
                }
            }

            // write to data buffer
            uint64_t start_addr = _data_written;
            _data_buffer.at(_data_written++) = hdr0;
            _data_buffer.at(_data_written++) = hdr1;

            if (generate_pattern) {
                for (uint64_t i = 0; i < content_words; i++) {
                    _data_buffer.at(_data_written++) = ((uint64_t) par->node_index() << 48) | i;
                }
            }

            // write to addr buffer
            _addr_buffer.at(_mc_written) = start_addr;
            {
                //                boost::unique_lock<boost::mutex> lock(_written_mutex);
                _mc_written++;
            }
            //_cond_written.notify_all();
            if (_is_stopped) return;
        }
    }


    //    to do: this is a bad idea (performance). minimize inter-thread comm!

    virtual void wait_for_data(uint64_t min_mc_number)
    {
        static uint64_t local_mc_written = 0;

        if (min_mc_number > local_mc_written) {
            //            boost::unique_lock<boost::mutex> lock(_written_mutex);
            while (min_mc_number > _mc_written) {
                //                _cond_written.wait(lock);
                boost::this_thread::yield();
            }
            local_mc_written = _mc_written;
        }
    };

    virtual void update_ack_pointers(uint64_t acked_data, uint64_t acked_mc)
    {
        {
            //            boost::unique_lock<boost::mutex> lock(_acked_mutex);
            _acked_data = acked_data;
            _acked_mc = acked_mc;
        }
        //        _cond_acked.notify_all();
    };

private:
    std::atomic<bool> _is_stopped{false};

    boost::thread* _generator_thread;

    boost::mutex _acked_mutex;
    boost::condition_variable _cond_acked;

    boost::mutex _written_mutex;
    boost::condition_variable _cond_written;

    /// Number of acknowledged data words. Updated by input node.
    std::atomic<uint64_t> _acked_data{0};
    
    /// Number of acknowledged MCs. Updated by input node.
    std::atomic<uint64_t> _acked_mc{0};
    
    /// FLIB-internal number of written data words. 
    uint64_t _data_written = 0;

    /// FLIB-internal number of written MCs. 
    std::atomic<uint64_t> _mc_written{0};

    /// A pseudo-random number generator.
    boost::mt19937 _rng;

    /// Distribution to use in determining data content sizes.
    boost::poisson_distribution<> _pd;

    /// Generate random number of content words.
    boost::variate_generator<boost::mt19937&,
                             boost::poisson_distribution<> > _rand_content_words;
};
    

#endif /* DATASOURCE_HPP */
