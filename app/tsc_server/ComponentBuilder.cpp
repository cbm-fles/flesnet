// Copyright 2025 Dirk Hutter

#include "ComponentBuilder.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "log.hpp"

template <typename T_DESC, typename T_DATA>
ComponentBuilder<T_DESC, T_DATA>::ComponentBuilder(
    ip::managed_shared_memory* shm,
    cri::cri_channel* cri_channel,
    size_t data_buffer_size_exp,
    size_t desc_buffer_size_exp)
    : m_shm(shm), m_cri_channel(cri_channel) {

  // allocate buffers in shm
  void* data_buffer_raw = alloc_buffer(data_buffer_size_exp, data_item_size);
  void* desc_buffer_raw = alloc_buffer(desc_buffer_size_exp, desc_item_size);

  // initialize cri DMA engine
  static_assert(desc_item_size == (UINT64_C(1) << 5),
                "incompatible desc_item_size in ComponentBuilder");
  static_assert(data_item_size == (UINT64_C(1) << 0),
                "incompatible data_item_size in ComponentBuilder");

  m_cri_channel->init_dma(data_buffer_raw, data_buffer_size_exp + 0,
                          desc_buffer_raw, desc_buffer_size_exp + 5);
  m_cri_channel->enable_readout();

  // initialize buffer interface
  m_cri_source_buffer = std::make_unique<cri_source<T_DESC, T_DATA>>(
      reinterpret_cast<T_DESC*>(desc_buffer_raw),
      reinterpret_cast<T_DATA*>(data_buffer_raw), desc_buffer_size_exp,
      data_buffer_size_exp, m_cri_channel->dma());
}

template <typename T_DESC, typename T_DATA>
ComponentBuilder<T_DESC, T_DATA>::~ComponentBuilder() {
  m_cri_channel->disable_readout();
  m_cri_channel->deinit_dma();
  // TODO deallocate buffers if it is worth to do
}

template <typename T_DESC, typename T_DATA>
void ComponentBuilder<T_DESC, T_DATA>::proceed() {
  // TODO: make this usefull
  DualIndex index = m_cri_source_buffer->get_write_index();
  m_cri_source_buffer->set_read_index(index);
  return;
}

template <typename T_DESC, typename T_DATA>
void* ComponentBuilder<T_DESC, T_DATA>::alloc_buffer(size_t size_exp,
                                                     size_t item_size) {
  size_t bytes = (UINT64_C(1) << size_exp) * item_size;
  L_(trace) << "allocating shm buffer of " << bytes << " bytes";
  return m_shm->allocate_aligned(bytes, sysconf(_SC_PAGESIZE));
}

template class ComponentBuilder<fles::MicrosliceDescriptor, uint8_t>;
