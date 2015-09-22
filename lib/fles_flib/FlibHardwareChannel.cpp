// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "FlibHardwareChannel.hpp"

FlibHardwareChannel::FlibHardwareChannel(std::size_t data_buffer_size_exp,
                                         std::size_t desc_buffer_size_exp,
                                         flib::flib_link* flib_link)
    : data_send_buffer_(data_buffer_size_exp),
      desc_send_buffer_(desc_buffer_size_exp),
      data_send_buffer_view_(data_send_buffer_.ptr(), data_buffer_size_exp),
      desc_send_buffer_view_(desc_send_buffer_.ptr(), desc_buffer_size_exp),
      flib_link_(flib_link)
{
    constexpr std::size_t microslice_descriptor_size_exp = 5;
    std::size_t desc_buffer_bytes_exp =
        desc_buffer_size_exp + microslice_descriptor_size_exp;

#ifndef NO_DOUBLE_BUFFERING
    flib_link_->init_dma(data_buffer_size_exp, desc_buffer_bytes_exp);

    uint8_t* data_buffer =
        reinterpret_cast<uint8_t*>(flib_link_->channel()->data_buffer());
    fles::MicrosliceDescriptor* desc_buffer =
        reinterpret_cast<fles::MicrosliceDescriptor*>(
            flib_link_->channel()->desc_buffer());

    data_buffer_view_ = std::unique_ptr<RingBufferView<volatile uint8_t>>(
        new RingBufferView<volatile uint8_t>(data_buffer,
                                             data_buffer_size_exp));
    desc_buffer_view_ =
        std::unique_ptr<RingBufferView<volatile fles::MicrosliceDescriptor>>(
            new RingBufferView<volatile fles::MicrosliceDescriptor>(
                desc_buffer, desc_buffer_size_exp));
#else
    flib_link_->init_dma(
        const_cast<void*>(static_cast<volatile void*>(data_send_buffer_.ptr())),
        data_buffer_size_exp,
        const_cast<void*>(static_cast<volatile void*>(desc_send_buffer_.ptr())),
        desc_buffer_bytes_exp);

    data_buffer_view_ = std::unique_ptr<RingBufferView<volatile uint8_t>>(
        new RingBufferView<volatile uint8_t>(data_send_buffer_.ptr(),
                                             data_buffer_size_exp));
    desc_buffer_view_ =
        std::unique_ptr<RingBufferView<volatile fles::MicrosliceDescriptor>>(
            new RingBufferView<volatile fles::MicrosliceDescriptor>(
                desc_send_buffer_.ptr(), desc_buffer_size_exp));
#endif

    flib_link_->set_start_idx(0);

    flib_link_->enable_readout(true);

    // assert(flib_link_->mc_index() == 0);
    // assert(flib_link_->pending_mc() == 0);
}

FlibHardwareChannel::~FlibHardwareChannel()
{
    flib_link_->rst_pending_mc();
    flib_link_->deinit_dma();
}

DualRingBufferIndex FlibHardwareChannel::get_write_index()
{
    return {flib_link_->mc_index(), flib_link_->channel()->get_data_offset()};
}

void FlibHardwareChannel::set_read_index(DualRingBufferIndex new_read_index)
{
    flib_link_->channel()->set_sw_read_pointers(
        new_read_index.data & data_buffer_view_->size_mask(),
        (new_read_index.desc & desc_buffer_view_->size_mask()) *
            sizeof(fles::MicrosliceDescriptor));
}
