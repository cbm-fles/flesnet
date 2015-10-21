// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "RingBufferView.hpp"
#include "MicrosliceDescriptor.hpp"

struct DualIndex {
    uint64_t desc;
    uint64_t data;
};

/// Abstract FLES data source class.
template <typename T_DESC, typename T_DATA> class DualRingBufferReadInterface
{
public:
    virtual ~DualRingBufferReadInterface() {}

    virtual void proceed() {}

    virtual DualIndex get_write_index() = 0;

    virtual void set_read_index(DualIndex new_read_index) = 0;

    virtual RingBufferView<T_DATA>& data_buffer() = 0;
    virtual RingBufferView<T_DESC>& desc_buffer() = 0;

    virtual RingBufferView<T_DATA>& data_send_buffer() { return data_buffer(); }
    virtual RingBufferView<T_DESC>& desc_send_buffer() { return desc_buffer(); }

    virtual void copy_to_data_send_buffer(std::size_t /* start */,
                                          std::size_t /* count */)
    {
    }

    virtual void copy_to_desc_send_buffer(std::size_t /* start */,
                                          std::size_t /* count */)
    {
    }
};

template <typename T_DESC, typename T_DATA> class DualRingBufferWriteInterface
{
public:
    virtual ~DualRingBufferWriteInterface() {}

    virtual DualIndex get_read_index() = 0;

    virtual void set_write_index(DualIndex new_write_index) = 0;

    virtual RingBufferView<T_DATA>& data_buffer() = 0;
    virtual RingBufferView<T_DESC>& desc_buffer() = 0;
};

using InputBufferReadInterface =
    DualRingBufferReadInterface<volatile fles::MicrosliceDescriptor,
                                volatile uint8_t>;

using InputBufferWriteInterface =
    DualRingBufferWriteInterface<volatile fles::MicrosliceDescriptor,
                                 volatile uint8_t>;
