#pragma once
/**
 * \file FlibPatternGenerator.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "DataSource.hpp"
#include "ThreadContainer.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
#include "global.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <algorithm>

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
                         bool randomize_sizes)
        : _data_buffer(data_buffer_size_exp),
          _desc_buffer(desc_buffer_size_exp),
          _data_send_buffer(data_buffer_size_exp),
          _desc_send_buffer(desc_buffer_size_exp),
          _data_buffer_view(_data_buffer.ptr(), data_buffer_size_exp),
          _desc_buffer_view(_desc_buffer.ptr(), desc_buffer_size_exp),
          _data_send_buffer_view(_data_send_buffer.ptr(), data_buffer_size_exp),
          _desc_send_buffer_view(_desc_send_buffer.ptr(), desc_buffer_size_exp),
          _input_index(input_index),
          _generate_pattern(generate_pattern),
          _typical_content_size(typical_content_size),
          _randomize_sizes(randomize_sizes)
    {
        _producer_thread
            = new std::thread(&FlibPatternGenerator::produce_data, this);
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

    virtual RingBufferView<>& data_buffer() override
    {
        return _data_buffer_view;
    }

    virtual RingBufferView<MicrosliceDescriptor>& desc_buffer() override
    {
        return _desc_buffer_view;
    }

    virtual RingBufferView<>& data_send_buffer() override
    {
        return _data_send_buffer_view;
    }

    virtual RingBufferView<MicrosliceDescriptor>& desc_send_buffer() override
    {
        return _desc_send_buffer_view;
    }

    /// Generate FLIB input data.
    void produce_data();

    virtual uint64_t wait_for_data(uint64_t min_mc_number) override
    {
        ++DCOUNT[3];
        std::unique_lock<std::mutex> l(_mutex);
        while (min_mc_number > _written_mc) {
            ++DCOUNT[4];
            _cond_consumer.wait(l);
        }
        return _written_mc;
    }

    virtual void update_ack_pointers(uint64_t new_acked_data,
                                     uint64_t new_acked_mc) override
    {
        ++DCOUNT[6];
        {
            std::unique_lock<std::mutex> l(_mutex);
            _acked_data = new_acked_data;
            _acked_mc = new_acked_mc;
        }
        _cond_producer.notify_one();
    }

    virtual void copy_to_data_send_buffer(std::size_t start, std::size_t count)
        override
    {
        std::copy_n(&_data_buffer_view.at(start),
                    count,
                    &_data_send_buffer_view.at(start));
    }

    virtual void copy_to_desc_send_buffer(std::size_t start, std::size_t count)
        override
    {
        std::copy_n(&_desc_buffer_view.at(start),
                    count,
                    &_desc_send_buffer_view.at(start));
    }

private:
    uint64_t DCOUNT[10] = {};

    /// Input data buffer.
    RingBuffer<> _data_buffer;

    /// Input descriptor buffer.
    RingBuffer<MicrosliceDescriptor> _desc_buffer;

    RingBuffer<> _data_send_buffer;
    RingBuffer<MicrosliceDescriptor> _desc_send_buffer;

    RingBufferView<> _data_buffer_view;
    RingBufferView<MicrosliceDescriptor> _desc_buffer_view;

    RingBufferView<> _data_send_buffer_view;
    RingBufferView<MicrosliceDescriptor> _desc_send_buffer_view;

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
