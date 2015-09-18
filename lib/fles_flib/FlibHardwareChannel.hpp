// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "RingBufferReadInterface.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
#include "MicrosliceDescriptor.hpp"
#include <unistd.h> // <- TODO: required due to bug in flib.h
#include <flib.h>
#include <algorithm>

#define NO_DOUBLE_BUFFERING

/// Wrapper around FLIB hardware channel.
class FlibHardwareChannel : public DualRingBufferReadInterface
{
public:
    /// The FlibHardwareChannel constructor.
    FlibHardwareChannel(std::size_t data_buffer_size_exp,
                        std::size_t desc_buffer_size_exp,
                        flib::flib_link* flib_link);

    FlibHardwareChannel(const FlibHardwareChannel&) = delete;
    void operator=(const FlibHardwareChannel&) = delete;

    virtual ~FlibHardwareChannel();

    virtual RingBufferView<volatile uint8_t>& data_buffer() override
    {
        return *data_buffer_view_;
    }

    virtual RingBufferView<volatile fles::MicrosliceDescriptor>&
    desc_buffer() override
    {
        return *desc_buffer_view_;
    }

    virtual RingBufferView<volatile uint8_t>& data_send_buffer() override
    {
        return data_send_buffer_view_;
    }

    virtual RingBufferView<volatile fles::MicrosliceDescriptor>&
    desc_send_buffer() override
    {
        return desc_send_buffer_view_;
    }

#ifndef NO_DOUBLE_BUFFERING
    virtual void copy_to_data_send_buffer(std::size_t start,
                                          std::size_t count) override
    {
        std::copy_n(const_cast<uint8_t*>(&data_buffer_view_->at(start)), count,
                    const_cast<uint8_t*>(&data_send_buffer_view_.at(start)));
    }

    virtual void copy_to_desc_send_buffer(std::size_t start,
                                          std::size_t count) override
    {
        std::copy_n(const_cast<fles::MicrosliceDescriptor*>(
                        &desc_buffer_view_->at(start)),
                    count, const_cast<fles::MicrosliceDescriptor*>(
                               &desc_send_buffer_view_.at(start)));
    }
#endif

    virtual DualRingBufferIndex get_write_index() override;

    virtual void set_read_index(DualRingBufferIndex new_read_index) override;

private:
    std::unique_ptr<RingBufferView<volatile uint8_t>> data_buffer_view_;
    std::unique_ptr<RingBufferView<volatile fles::MicrosliceDescriptor>>
        desc_buffer_view_;

    RingBuffer<volatile uint8_t, true, true> data_send_buffer_;
    RingBuffer<volatile fles::MicrosliceDescriptor, true, true>
        desc_send_buffer_;

    RingBufferView<volatile uint8_t> data_send_buffer_view_;
    RingBufferView<volatile fles::MicrosliceDescriptor> desc_send_buffer_view_;

    /// Associated FLIB link class.
    flib::flib_link* flib_link_;
};
