/**
 * \file FlibHardwareChannel.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "FlibHardwareChannel.hpp"
#include <chrono>
#include <thread>

FlibHardwareChannel::FlibHardwareChannel(std::size_t data_buffer_size_exp,
                                         std::size_t desc_buffer_size_exp,
                                         uint64_t input_index,
                                         flib::flib_link* flib_link)
    : _input_index(input_index), _flib_link(flib_link)
{
    _flib_link->init_dma(
        flib::create_only, data_buffer_size_exp, desc_buffer_size_exp);
    // TODO?: reduce desc_buffer_size_exp (for sizeof MicrosliceDescriptor)?

    uint8_t* data_buffer = reinterpret_cast
        <uint8_t*>(_flib_link->ebuf()->getMem());
    MicrosliceDescriptor* desc_buffer = reinterpret_cast
        <MicrosliceDescriptor*>(_flib_link->rbuf()->getMem());

    _data_buffer_view = std::unique_ptr<RingBufferView<> >(
        new RingBufferView<>(data_buffer, data_buffer_size_exp));
    _desc_buffer_view = std::unique_ptr<RingBufferView<MicrosliceDescriptor> >(
        new RingBufferView
        <MicrosliceDescriptor>(desc_buffer, desc_buffer_size_exp));

    flib::hdr_config config{static_cast<uint16_t>(0xE000 + input_index), 0xBC,
                            0xFD};
    _flib_link->set_hdr_config(&config);

    _flib_link->rst_pending_mc();

    _flib_link->set_data_rx_sel(flib::flib_link::pgen);
    _flib_link->enable_cbmnet_packer(true);
}

FlibHardwareChannel::~FlibHardwareChannel()
{
    _flib_link->rst_pending_mc();
}

uint64_t FlibHardwareChannel::wait_for_data(uint64_t min_mc_number)
{
    uint64_t hw_mc_index = 0;
    std::chrono::microseconds interval(100);

    while ((hw_mc_index = _flib_link->get_mc_index()) < min_mc_number) {
        std::this_thread::sleep_for(interval);
    }

    return hw_mc_index;
}

void FlibHardwareChannel::update_ack_pointers(uint64_t new_acked_data,
                                              uint64_t new_acked_mc)
{
    _flib_link->get_ch()->setOffsets(
        new_acked_data & _data_buffer_view->size_mask(),
        (new_acked_mc & _desc_buffer_view->size_mask())
        * sizeof(MicrosliceDescriptor));
}
