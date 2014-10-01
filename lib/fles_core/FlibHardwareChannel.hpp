// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DataSource.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
#include "MicrosliceDescriptor.hpp"
#include <unistd.h> // <- TODO: required due to bug in flib.h
#include <flib.h>
#include <algorithm>

/// Wrapper around FLIB hardware channel.
class FlibHardwareChannel : public DataSource
{
public:
    /// The FlibHardwareChannel constructor.
    FlibHardwareChannel(std::size_t data_buffer_size_exp,
                        std::size_t desc_buffer_size_exp,
                        flib::flib_link* flib_link);

    FlibHardwareChannel(const FlibHardwareChannel&) = delete;
    void operator=(const FlibHardwareChannel&) = delete;

    virtual ~FlibHardwareChannel();

    virtual RingBufferView<>& data_buffer() override
    {
        return *_data_buffer_view;
    }

    virtual RingBufferView<fles::MicrosliceDescriptor>& desc_buffer() override
    {
        return *_desc_buffer_view;
    }

    virtual RingBufferView<>& data_send_buffer() override
    {
        return _data_send_buffer_view;
    }

    virtual RingBufferView<fles::MicrosliceDescriptor>& desc_send_buffer()
        override
    {
        return _desc_send_buffer_view;
    }

#ifndef NO_DOUBLE_BUFFERING
    virtual void copy_to_data_send_buffer(std::size_t start, std::size_t count)
        override
    {
        std::copy_n(&_data_buffer_view->at(start), count,
                    &_data_send_buffer_view.at(start));
    }

    virtual void copy_to_desc_send_buffer(std::size_t start, std::size_t count)
        override
    {
        std::copy_n(&_desc_buffer_view->at(start), count,
                    &_desc_send_buffer_view.at(start));
    }
#endif

    virtual uint64_t written_mc() override;
    virtual uint64_t written_data() override;

    virtual void update_ack_pointers(uint64_t new_acked_data,
                                     uint64_t new_acked_mc) override;

private:
    std::unique_ptr<RingBufferView<>> _data_buffer_view;
    std::unique_ptr<RingBufferView<fles::MicrosliceDescriptor>>
    _desc_buffer_view;

    RingBuffer<> _data_send_buffer;
    RingBuffer<fles::MicrosliceDescriptor> _desc_send_buffer;

    RingBufferView<> _data_send_buffer_view;
    RingBufferView<fles::MicrosliceDescriptor> _desc_send_buffer_view;

    /// Associated FLIB link class.
    flib::flib_link* _flib_link;
};
