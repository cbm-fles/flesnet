// Copyright 2025 Dirk Hutter

#include "cri_source.hpp"
#include "log.hpp"

template <typename T_DESC, typename T_DATA>
cri_source<T_DESC, T_DATA>::cri_source(T_DESC* desc_buffer,
                                       T_DATA* data_buffer,
                                       size_t desc_buffer_size_exp,
                                       size_t data_buffer_size_exp,
                                       cri::dma_channel* dma_channel)
    : m_desc_buffer_size_exp(desc_buffer_size_exp),
      m_data_buffer_size_exp(data_buffer_size_exp), m_dma_channel(dma_channel),
      m_dma_transfer_size(dma_channel->dma_transfer_size()) {
  // TODO: we can possibly extract all buffer information from dma_channel
  // (or cri_channel), especially if size in not a exponent

  static_assert(desc_item_size == (UINT64_C(1) << 5),
                "incompatible desc_item_size in cri_source");
  static_assert(data_item_size == (UINT64_C(1) << 0),
                "incompatible data_item_size in cri_source");

  m_desc_buffer_view = std::unique_ptr<RingBufferView<T_DESC>>(
      new RingBufferView<T_DESC>(desc_buffer, m_desc_buffer_size_exp));
  m_data_buffer_view = std::unique_ptr<RingBufferView<T_DATA>>(
      new RingBufferView<T_DATA>(data_buffer, m_data_buffer_size_exp));
}

template <typename T_DESC, typename T_DATA>
cri_source<T_DESC, T_DATA>::~cri_source() {}

template <typename T_DESC, typename T_DATA>
void cri_source<T_DESC, T_DATA>::set_read_index(DualIndex read_index) {
  L_(trace) << "updating read_index: data " << read_index.data << " desc "
            << read_index.desc;

  m_dma_channel->set_sw_read_pointers(
      hw_pointer(read_index.data, m_data_buffer_size_exp, data_item_size,
                 m_dma_transfer_size),
      hw_pointer(read_index.desc, m_desc_buffer_size_exp, desc_item_size));

  // cache the read_index locally
  m_read_index = read_index;
}

template <typename T_DESC, typename T_DATA>
DualIndex cri_source<T_DESC, T_DATA>::get_read_index() {
  // We only return the locally cached index and do not consult the hardware
  return m_read_index;
}

template <typename T_DESC, typename T_DATA>
DualIndex cri_source<T_DESC, T_DATA>::get_write_index() {
  DualIndex write_index;
  write_index.desc = m_dma_channel->get_desc_index();
  // write index points to the end of an element, so we substract 1 to access
  // the last desc
  write_index.data = m_desc_buffer_view->at(write_index.desc - 1).offset +
                     m_desc_buffer_view->at(write_index.desc - 1).size;
  L_(trace) << "fetching write_index: data " << write_index.data << " desc "
            << write_index.desc;
  return write_index;
}

template <typename T_DESC, typename T_DATA>
DualIndexTimed cri_source<T_DESC, T_DATA>::get_write_index_timed() {
  DualIndexTimed write_index;
  write_index.updated = std::chrono::high_resolution_clock::now();
  write_index.index = get_write_index();
  return write_index;
}

template <typename T_DESC, typename T_DATA>
bool cri_source<T_DESC, T_DATA>::get_eof() {
  // TODO: no idea who should ever set eof, should this check if something in hw
  // is enabled?
  return false;
}

// TODO: index to offset calculations could also be done by the RingBuffer class
template <typename T_DESC, typename T_DATA>
size_t cri_source<T_DESC, T_DATA>::hw_pointer(size_t index,
                                              size_t size_exponent,
                                              size_t item_size) {
  size_t buffer_size = UINT64_C(1) << size_exponent;
  size_t masked_index = index & (buffer_size - 1);
  return masked_index * item_size;
}

template <typename T_DESC, typename T_DATA>
size_t cri_source<T_DESC, T_DATA>::hw_pointer(size_t index,
                                              size_t size_exponent,
                                              size_t item_size,
                                              size_t dma_size) {
  size_t byte_index = hw_pointer(index, size_exponent, item_size);
  // will hang one transfer size behind
  return byte_index & ~(dma_size - 1);
}

template class cri_source<fles::MicrosliceDescriptor, uint8_t>;
