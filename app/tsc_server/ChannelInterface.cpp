// Copyright 2025 Dirk Hutter

#include "ChannelInterface.hpp"
#include "log.hpp"
#include <cstddef>
#include <sys/types.h>

ChannelInterface::ChannelInterface(fles::MicrosliceDescriptor* desc_buffer,
                                   uint8_t* data_buffer,
                                   size_t desc_buffer_size_exp,
                                   size_t data_buffer_size_exp,
                                   cri::dma_channel* dma_channel)
    : m_dma_channel(dma_channel),
      m_dma_transfer_size(dma_channel->dma_transfer_size()) {
  // TODO: we can possibly extract all buffer information from dma_channel
  // (or cri_channel), especially if size in not an exponent

  m_desc_buffer_view =
      std::make_unique<RingBufferView<fles::MicrosliceDescriptor>>(
          desc_buffer, desc_buffer_size_exp);
  m_data_buffer_view = std::make_unique<RingBufferView<uint8_t>>(
      data_buffer, data_buffer_size_exp);

  // TODO: intialize m_read_index to the current hardware read index
}

void ChannelInterface::set_read_index(uint64_t read_index) {
  uint64_t data_read_index = m_desc_buffer_view->at(read_index - 1).offset +
                             m_desc_buffer_view->at(read_index - 1).size;

  if (read_index == m_read_index) {
    L_(trace) << "updating read_index, nothing to do for: data "
              << data_read_index << " desc " << read_index;
    return;
  }

  if (read_index < m_read_index) {
    std::stringstream ss;
    ss << "new read index " << data_read_index << " desc " << read_index
       << " is smaller than the current read index desc " << m_read_index
       << ", this should not happen!";
    throw std::runtime_error(ss.str());
  }

  L_(trace) << "updating read_index: data " << data_read_index << " desc "
            << read_index;

  m_dma_channel->set_sw_read_pointers(
      hw_pointer(data_read_index, m_data_buffer_view->size_exponent(),
                 sizeof(uint8_t), m_dma_transfer_size),
      hw_pointer(read_index, m_desc_buffer_view->size_exponent(),
                 sizeof(fles::MicrosliceDescriptor)));

  // cache the read_index locally
  m_read_index = read_index;
}

uint64_t ChannelInterface::get_read_index() const {
  // We only return the locally cached index and do not consult the hardware
  return m_read_index;
}

uint64_t ChannelInterface::get_write_index() {
  return m_dma_channel->get_desc_index();
}

// TODO: index to offset calculations could also be done by the RingBuffer class
size_t ChannelInterface::hw_pointer(size_t index,
                                    size_t size_exponent,
                                    size_t item_size) {
  size_t buffer_size = UINT64_C(1) << size_exponent;
  size_t masked_index = index & (buffer_size - 1);
  return masked_index * item_size;
}

size_t ChannelInterface::hw_pointer(size_t index,
                                    size_t size_exponent,
                                    size_t item_size,
                                    size_t dma_size) {
  size_t byte_index = hw_pointer(index, size_exponent, item_size);
  // will hang one transfer size behind
  return byte_index & ~(dma_size - 1);
}
