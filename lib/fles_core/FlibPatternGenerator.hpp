// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "RingBufferReadInterface.hpp"
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
class FlibPatternGenerator : public InputBufferReadInterface,
                             public ThreadContainer
{
public:
    /// The FlibPatternGenerator constructor.
    FlibPatternGenerator(std::size_t data_buffer_size_exp,
                         std::size_t desc_buffer_size_exp, uint64_t input_index,
                         uint32_t typical_content_size)
        : data_buffer_(data_buffer_size_exp),
          desc_buffer_(desc_buffer_size_exp),
          data_buffer_view_(data_buffer_.ptr(), data_buffer_size_exp),
          desc_buffer_view_(desc_buffer_.ptr(), desc_buffer_size_exp),
          input_index_(input_index), generate_pattern_(false),
          typical_content_size_(typical_content_size), randomize_sizes_(false)
    {
        producer_thread_ =
            new std::thread(&FlibPatternGenerator::produce_data, this);
    }

    FlibPatternGenerator(const FlibPatternGenerator&) = delete;
    void operator=(const FlibPatternGenerator&) = delete;

    virtual ~FlibPatternGenerator()
    {
        try {
            is_stopped_ = true;
            producer_thread_->join();
            delete producer_thread_;
        } catch (std::exception& e) {
            L_(error) << "exception in destructor ~FlibPatternGenerator(): "
                      << e.what();
        }
    }

    virtual RingBufferView<volatile uint8_t>& data_buffer() override
    {
        return data_buffer_view_;
    }

    virtual RingBufferView<volatile fles::MicrosliceDescriptor>&
    desc_buffer() override
    {
        return desc_buffer_view_;
    }

    /// Generate FLIB input data.
    void produce_data();

    virtual DualRingBufferIndex get_write_index() override
    {
        return write_index_.load();
    }

    virtual void set_read_index(DualRingBufferIndex new_read_index) override
    {
        read_index_.store(new_read_index);
    }

private:
    /// Input data buffer.
    RingBuffer<volatile uint8_t> data_buffer_;

    /// Input descriptor buffer.
    RingBuffer<volatile fles::MicrosliceDescriptor, true> desc_buffer_;

    RingBufferView<volatile uint8_t> data_buffer_view_;
    RingBufferView<volatile fles::MicrosliceDescriptor> desc_buffer_view_;

    /// This node's index in the list of input nodes
    uint64_t input_index_;

    bool generate_pattern_;
    uint32_t typical_content_size_;
    bool randomize_sizes_;

    std::atomic<bool> is_stopped_{false};

    std::thread* producer_thread_;

    /// Number of acknowledged data bytes and microslices. Updated by input
    /// node.
    std::atomic<DualRingBufferIndex> read_index_{{0, 0}};

    /// FLIB-internal number of written microslices and data bytes.
    std::atomic<DualRingBufferIndex> write_index_{{0, 0}};
};
