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
        : _data_buffer(data_buffer_size_exp),
          _desc_buffer(desc_buffer_size_exp),
          _data_buffer_view(_data_buffer.ptr(), data_buffer_size_exp),
          _desc_buffer_view(_desc_buffer.ptr(), desc_buffer_size_exp),
          _input_index(input_index), _generate_pattern(false),
          _typical_content_size(typical_content_size), _randomize_sizes(false),
          _random_distribution(typical_content_size)
    {
    }

    EmbeddedPatternGenerator(const EmbeddedPatternGenerator&) = delete;
    void operator=(const EmbeddedPatternGenerator&) = delete;

    virtual RingBufferView<volatile uint8_t>& data_buffer() override
    {
        return _data_buffer_view;
    }

    virtual RingBufferView<volatile fles::MicrosliceDescriptor>&
    desc_buffer() override
    {
        return _desc_buffer_view;
    }

    void proceed();

    virtual uint64_t written_desc() override { return _written_desc; }
    virtual uint64_t written_data() override { return _written_data; }

    virtual void update_ack_pointers(uint64_t new_acked_data,
                                     uint64_t new_acked_desc) override
    {
        _acked_data = new_acked_data;
        _acked_desc = new_acked_desc;
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

    /// A pseudo-random number generator.
    std::default_random_engine _random_generator;

    /// Distribution to use in determining data content sizes.
    std::poisson_distribution<unsigned int> _random_distribution;

    /// Number of acknowledged data bytes. Updated by input node.
    uint64_t _acked_data{0};

    /// Number of acknowledged MCs. Updated by input node.
    uint64_t _acked_desc{0};

    /// FLIB-internal number of written data bytes.
    uint64_t _written_data{0};

    /// FLIB-internal number of written MCs.
    uint64_t _written_desc{0};
};
