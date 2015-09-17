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
class FlibPatternGenerator : public RingBufferReadInterface,
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

    virtual uint64_t written_desc() override { return written_desc_; }
    virtual uint64_t written_data() override { return written_data_; }

    virtual void update_ack_pointers(uint64_t new_acked_data,
                                     uint64_t new_acked_desc) override
    {
        acked_data_ = new_acked_data;
        acked_desc_ = new_acked_desc;
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

    /// Number of acknowledged data bytes. Updated by input node.
    std::atomic<uint64_t> acked_data_{0};

    /// Number of acknowledged microslices. Updated by input node.
    std::atomic<uint64_t> acked_desc_{0};

    /// FLIB-internal number of written data bytes.
    uint64_t written_data_{0};

    /// FLIB-internal number of written microslices.
    std::atomic<uint64_t> written_desc_{0};
};
