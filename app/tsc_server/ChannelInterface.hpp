// Copyright 2025 Dirk Hutter

#pragma once

#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"
#include "dma_channel.hpp"
#include <chrono>
#include <memory>

class ChannelInterface {
public:
  ChannelInterface(fles::MicrosliceDescriptor* desc_buffer,
                   uint8_t* data_buffer,
                   size_t desc_buffer_size_exp,
                   size_t data_buffer_size_exp,
                   cri::dma_channel* dma_channel);
  ChannelInterface(const ChannelInterface&) = delete;
  void operator=(const ChannelInterface&) = delete;

  void set_read_index(uint64_t desc_read_index);

  [[nodiscard]] uint64_t get_read_index() const;

  [[nodiscard]] uint64_t get_write_index();

  RingBufferView<uint8_t>& data_buffer() { return *m_data_buffer_view; }
  RingBufferView<fles::MicrosliceDescriptor>& desc_buffer() {
    return *m_desc_buffer_view;
  }

private:
  // Convert index into byte pointer for hardware
  size_t hw_pointer(size_t index, size_t size_exponent, size_t item_size);
  // Convert index into byte pointer and round to dma_size
  size_t hw_pointer(size_t index,
                    size_t size_exponent,
                    size_t item_size,
                    size_t dma_size);

  cri::dma_channel* m_dma_channel;
  size_t m_dma_transfer_size;

  std::unique_ptr<RingBufferView<fles::MicrosliceDescriptor>>
      m_desc_buffer_view;
  std::unique_ptr<RingBufferView<uint8_t>> m_data_buffer_view;

  // TODO: in case of reconnects this has to be initialized with the hw value
  uint64_t m_read_index = 0; // INFO not actual hw value
};
