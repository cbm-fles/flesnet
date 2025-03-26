// Copyright 2025 Dirk Hutter

#pragma once

#include "DualRingBuffer.hpp"
#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"
#include "dma_channel.hpp"
#include <memory>

template <typename T_DESC, typename T_DATA>
class cri_source : public DualRingBufferReadInterface<T_DESC, T_DATA> {

public:
  cri_source(T_DESC* desc_buffer,
             T_DATA* data_buffer,
             size_t desc_buffer_size_exp,
             size_t data_buffer_size_exp,
             cri::dma_channel* dma_channel);
  cri_source(const cri_source&) = delete;
  void operator=(const cri_source&) = delete;

  ~cri_source() override;

  void set_read_index(DualIndex read_index) override;

  DualIndex get_read_index() override;

  DualIndex get_write_index() override;

  bool get_eof() override;

  size_t data_buffer_size_exp() { return m_data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return m_desc_buffer_size_exp; }

  RingBufferView<T_DATA>& data_buffer() override { return *m_data_buffer_view; }
  RingBufferView<T_DESC>& desc_buffer() override { return *m_desc_buffer_view; }

private:
  // Convert index into byte pointer for hardware
  size_t hw_pointer(size_t index, size_t size_exponent, size_t item_size);
  // Convert index into byte pointer and round to dma_size
  size_t hw_pointer(size_t index,
                    size_t size_exponent,
                    size_t item_size,
                    size_t dma_size);

  size_t m_desc_buffer_size_exp;
  size_t m_data_buffer_size_exp;

  cri::dma_channel* m_dma_channel;
  size_t m_dma_transfer_size;

  std::unique_ptr<RingBufferView<T_DESC>> m_desc_buffer_view;
  std::unique_ptr<RingBufferView<T_DATA>> m_data_buffer_view;

  // TODO: chould be initialized with hw value?
  DualIndex m_read_index{0, 0}; // INFO not actual hw value

  constexpr static size_t data_item_size = sizeof(T_DATA);
  constexpr static size_t desc_item_size = sizeof(T_DESC);
};
