#pragma once
/**
 * \file FlibHardwareChannel.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "DataSource.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
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
                        uint64_t input_index,
                        flib::flib_link* flib_link);

    FlibHardwareChannel(const FlibHardwareChannel&) = delete;
    void operator=(const FlibHardwareChannel&) = delete;

    ~FlibHardwareChannel();

    virtual RingBufferView<>& data_buffer() override
    {
        return *_data_buffer_view;
    }

    virtual RingBufferView<MicrosliceDescriptor>& desc_buffer() override
    {
        return *_desc_buffer_view;
    }

    virtual RingBufferView<>& data_send_buffer() override
    {
        return _data_send_buffer_view;
    }

    virtual RingBufferView<MicrosliceDescriptor>& desc_send_buffer() override
    {
        return _desc_send_buffer_view;
    }

    virtual void copy_to_data_send_buffer(std::size_t start, std::size_t count)
        override
    {
        std::copy_n(&_data_buffer_view->at(start),
                    count,
                    &_data_send_buffer_view.at(start));
    }

    virtual void copy_to_desc_send_buffer(std::size_t start, std::size_t count)
        override
    {
        std::copy_n(&_desc_buffer_view->at(start),
                    count,
                    &_desc_send_buffer_view.at(start));
    }

    virtual uint64_t wait_for_data(uint64_t min_mc_number) override;

    virtual void update_ack_pointers(uint64_t new_acked_data,
                                     uint64_t new_acked_mc) override;

private:
    std::unique_ptr<RingBufferView<> > _data_buffer_view;
    std::unique_ptr<RingBufferView<MicrosliceDescriptor> > _desc_buffer_view;

    RingBuffer<> _data_send_buffer;
    RingBuffer<MicrosliceDescriptor> _desc_send_buffer;

    RingBufferView<> _data_send_buffer_view;
    RingBufferView<MicrosliceDescriptor> _desc_send_buffer_view;

    /// This node's index in the list of input nodes
    uint64_t _input_index;

    /// Associated FLIB link class.
    flib::flib_link* _flib_link;

    /// Number of acknowledged data bytes. Updated by input node.
    uint64_t _acked_data{0};

    /// Number of acknowledged MCs. Updated by input node.
    uint64_t _acked_mc{0};

    /// FLIB-internal number of written data bytes.
    uint64_t _written_data{0};

    /// FLIB-internal number of written MCs.
    uint64_t _written_mc{0};
};
