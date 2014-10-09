// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DataSource.hpp"
#include "ThreadContainer.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
#include "MicrosliceDescriptor.hpp"
#include "log.hpp"
#include <atomic>
#include <thread>
#include <random>
#include <algorithm>

/// Simple software pattern generator used as FLIB replacement.
class FlibPatternGenerator : public DataSource, public ThreadContainer
{
public:
    /// The FlibPatternGenerator constructor.
    FlibPatternGenerator(std::size_t data_buffer_size_exp,
                         std::size_t desc_buffer_size_exp, uint64_t input_index,
                         uint32_t typical_content_size)
        : _data_buffer(data_buffer_size_exp),
          _desc_buffer(desc_buffer_size_exp),
          _data_buffer_view(_data_buffer.ptr(), data_buffer_size_exp),
          _desc_buffer_view(_desc_buffer.ptr(), desc_buffer_size_exp),
          _input_index(input_index), _generate_pattern(true),
          _typical_content_size(typical_content_size), _randomize_sizes(true)
    {
        _producer_thread =
            new std::thread(&FlibPatternGenerator::produce_data, this);
    }

    FlibPatternGenerator(const FlibPatternGenerator&) = delete;
    void operator=(const FlibPatternGenerator&) = delete;

    virtual ~FlibPatternGenerator()
    {
        try {
            _is_stopped = true;
            _producer_thread->join();
            delete _producer_thread;
        } catch (std::exception& e) {
            L_(error) << "exception in destructor ~FlibPatternGenerator(): "
                      << e.what();
        }
    }

    virtual RingBufferView<volatile uint8_t>& data_buffer() override
    {
        return _data_buffer_view;
    }

    virtual RingBufferView<volatile fles::MicrosliceDescriptor>&
    desc_buffer() override
    {
        return _desc_buffer_view;
    }

    /// Generate FLIB input data.
    void produce_data();

    virtual uint64_t written_mc() override { return _written_mc; }
    virtual uint64_t written_data() override { return _written_data; }

    virtual void update_ack_pointers(uint64_t new_acked_data,
                                     uint64_t new_acked_mc) override
    {
        _acked_data = new_acked_data;
        _acked_mc = new_acked_mc;
    }

private:
    /// Input data buffer.
    RingBuffer<volatile uint8_t> _data_buffer;

    /// Input descriptor buffer.
    RingBuffer<volatile fles::MicrosliceDescriptor> _desc_buffer;

    RingBufferView<volatile uint8_t> _data_buffer_view;
    RingBufferView<volatile fles::MicrosliceDescriptor> _desc_buffer_view;

    /// This node's index in the list of input nodes
    uint64_t _input_index;

    bool _generate_pattern;
    uint32_t _typical_content_size;
    bool _randomize_sizes;

    std::atomic<bool> _is_stopped{false};

    std::thread* _producer_thread;

    /// Number of acknowledged data bytes. Updated by input node.
    std::atomic<uint64_t> _acked_data{0};

    /// Number of acknowledged MCs. Updated by input node.
    std::atomic<uint64_t> _acked_mc{0};

    /// FLIB-internal number of written data bytes.
    uint64_t _written_data{0};

    /// FLIB-internal number of written MCs.
    std::atomic<uint64_t> _written_mc{0};
};
