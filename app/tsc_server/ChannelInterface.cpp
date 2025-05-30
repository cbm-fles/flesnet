// Copyright 2025 Dirk Hutter

#include "ChannelInterface.hpp"
#include "DualRingBuffer.hpp"
#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"
#include "log.hpp"
#include <cstddef>

ChannelInterface::ChannelInterface(fles::MicrosliceDescriptor* desc_buffer,
                                   uint8_t* data_buffer,
                                   size_t desc_buffer_size_exp,
                                   size_t data_buffer_size_exp,
                                   cri::dma_channel* dma_channel)
    : m_desc_buffer_size_exp(desc_buffer_size_exp),
      m_data_buffer_size_exp(data_buffer_size_exp), m_dma_channel(dma_channel),
      m_dma_transfer_size(dma_channel->dma_transfer_size()) {
  // TODO: we can possibly extract all buffer information from dma_channel
  // (or cri_channel), especially if size in not an exponent

  m_desc_buffer_view =
      std::make_unique<RingBufferView<fles::MicrosliceDescriptor>>(
          desc_buffer, m_desc_buffer_size_exp);
  m_data_buffer_view = std::make_unique<RingBufferView<uint8_t>>(
      data_buffer, m_data_buffer_size_exp);

  // TODO: intialize m_read_index to the current hardware read index
}

ChannelInterface::~ChannelInterface() {}

void ChannelInterface::set_read_index(DualIndex read_index) {
  if (read_index == m_read_index) {
    L_(trace) << "updating read_index, nothing to do for: data "
              << read_index.data << " desc " << read_index.desc;
    return;
  }

  if (read_index < m_read_index) {
    std::stringstream ss;
    ss << "new read index " << read_index.data << " desc " << read_index.desc
       << " is smaller than the current read index " << m_read_index.data
       << " desc " << m_read_index.desc << ", this should not happen!";
    throw std::runtime_error(ss.str());
  }

  L_(trace) << "updating read_index: data " << read_index.data << " desc "
            << read_index.desc;

  m_dma_channel->set_sw_read_pointers(
      hw_pointer(read_index.data, m_data_buffer_size_exp, sizeof(uint8_t),
                 m_dma_transfer_size),
      hw_pointer(read_index.desc, m_desc_buffer_size_exp,
                 sizeof(fles::MicrosliceDescriptor)));

  // cache the read_index locally
  m_read_index = read_index;
}

void ChannelInterface::set_read_index(uint64_t desc_read_index) {
  DualIndex read_index{};
  read_index.desc = desc_read_index;
  read_index.data = m_desc_buffer_view->at(read_index.desc - 1).offset +
                    m_desc_buffer_view->at(read_index.desc - 1).size;

  set_read_index(read_index);
}

DualIndex ChannelInterface::get_read_index() {
  // We only return the locally cached index and do not consult the hardware
  return m_read_index;
}

DualIndex ChannelInterface::get_write_index() {
  DualIndex write_index{};
  write_index.desc = m_dma_channel->get_desc_index();
  // write index points to the end of an element, so we substract 1 to access
  // the last desc
  write_index.data = m_desc_buffer_view->at(write_index.desc - 1).offset +
                     m_desc_buffer_view->at(write_index.desc - 1).size;
  L_(trace) << "fetching write_index: data " << write_index.data << " desc "
            << write_index.desc << " ms_time "
            << m_desc_buffer_view->at(write_index.desc - 1).idx;
  return write_index;
}

DualIndexTimed ChannelInterface::get_write_index_timed() {
  DualIndexTimed write_index;
  write_index.updated = std::chrono::high_resolution_clock::now();
  write_index.index = get_write_index();
  write_index.delta =
      (write_index.updated -
       (std::chrono::time_point<std::chrono::high_resolution_clock>(
           std::chrono::nanoseconds(
               m_desc_buffer_view->at(write_index.index.desc - 1).idx))));
  L_(trace) << "fetching write_index_timed: update time "
            << write_index.updated.time_since_epoch().count() << " delta "
            << write_index.delta.count();
  return write_index;
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
