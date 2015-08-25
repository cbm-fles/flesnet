// Copyright 2012-2014 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DataSource.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
#include "MicrosliceDescriptor.hpp"
#include "log.hpp"
#include <random>
#include <algorithm>

/// Simple embedded software pattern generator.
class EmbeddedPatternGenerator : public DataSource
{
public:
    /// The EmbeddedPatternGenerator constructor.
    EmbeddedPatternGenerator(std::size_t data_buffer_size_exp,
                             std::size_t desc_buffer_size_exp,
                             uint64_t input_index,
                             uint32_t typical_content_size)
        : data_buffer_(data_buffer_size_exp),
          desc_buffer_(desc_buffer_size_exp),
          data_buffer_view_(data_buffer_.ptr(), data_buffer_size_exp),
          desc_buffer_view_(desc_buffer_.ptr(), desc_buffer_size_exp),
          input_index_(input_index), generate_pattern_(false),
          typical_content_size_(typical_content_size), randomize_sizes_(false),
          random_distribution_(typical_content_size)
    {
    }

    EmbeddedPatternGenerator(const EmbeddedPatternGenerator&) = delete;
    void operator=(const EmbeddedPatternGenerator&) = delete;

    virtual RingBufferView<volatile uint8_t>& data_buffer() override
    {
        return data_buffer_view_;
    }

    virtual RingBufferView<volatile fles::MicrosliceDescriptor>&
    desc_buffer() override
    {
        return desc_buffer_view_;
    }

    void proceed();

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
    RingBuffer<volatile fles::MicrosliceDescriptor> desc_buffer_;

    RingBufferView<volatile uint8_t> data_buffer_view_;
    RingBufferView<volatile fles::MicrosliceDescriptor> desc_buffer_view_;

    /// This node's index in the list of input nodes
    uint64_t input_index_;

    bool generate_pattern_;
    uint32_t typical_content_size_;
    bool randomize_sizes_;

    /// A pseudo-random number generator.
    std::default_random_engine random_generator_;

    /// Distribution to use in determining data content sizes.
    std::poisson_distribution<unsigned int> random_distribution_;

    /// Number of acknowledged data bytes. Updated by input node.
    uint64_t acked_data_{0};

    /// Number of acknowledged microslices. Updated by input node.
    uint64_t acked_desc_{0};

    /// FLIB-internal number of written data bytes.
    uint64_t written_data_{0};

    /// FLIB-internal number of written microslices.
    uint64_t written_desc_{0};
};
