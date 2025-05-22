// Copyright 2025 Dirk Hutter

#include "ComponentBuilder.hpp"
#include "DualRingBuffer.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "log.hpp"

ComponentBuilder::ComponentBuilder(ip::managed_shared_memory* shm,
                                   cri::cri_channel* cri_channel,
                                   size_t data_buffer_size_exp,
                                   size_t desc_buffer_size_exp)
    : m_shm(shm), m_cri_channel(cri_channel) {

  // allocate buffers in shm
  void* data_buffer_raw = alloc_buffer(data_buffer_size_exp, sizeof(uint8_t));
  void* desc_buffer_raw =
      alloc_buffer(desc_buffer_size_exp, sizeof(fles::MicrosliceDescriptor));

  // initialize cri DMA engine
  m_cri_channel->init_dma(data_buffer_raw, data_buffer_size_exp + 0,
                          desc_buffer_raw, desc_buffer_size_exp + 5);
  m_cri_channel->enable_readout();

  // initialize buffer interface
  m_cri_source_buffer = std::make_unique<cri_source>(
      reinterpret_cast<fles::MicrosliceDescriptor*>(desc_buffer_raw),
      reinterpret_cast<uint8_t*>(data_buffer_raw), desc_buffer_size_exp,
      data_buffer_size_exp, m_cri_channel->dma());
}

ComponentBuilder::~ComponentBuilder() {
  m_cri_channel->disable_readout();
  m_cri_channel->deinit_dma();
  // TODO deallocate buffers if it is worth to do
}

void ComponentBuilder::proceed() {
  // TODO: make this usefull
  DualIndexTimed index = m_cri_source_buffer->get_write_index_timed();
  m_cri_source_buffer->set_read_index(index.index);

  L_(status) << "Fetching microslice: data " << index.index.data << " desc "
             << index.index.desc << " update time "
             << index.updated.time_since_epoch().count() << " delta "
             << index.delta.count();

  return;
}
void ComponentBuilder::ack_before(uint64_t time) {
  // fetch the current read and write index and initialize iterators
  // INFO: iterator desc_end points (as the write index does) to the next
  // element after the last valid element and must not be dereferenced
  // TODO: we may want to introduce a cached write index as member of builder
  // and rely on other call to update it from HW
  DualIndex write_index = m_cri_source_buffer->get_write_index();
  DualIndex read_index = m_cri_source_buffer->get_read_index();
  auto desc_begin =
      m_cri_source_buffer->desc_buffer().get_iter(read_index.desc);
  auto desc_end = m_cri_source_buffer->desc_buffer().get_iter(write_index.desc);

  // find the first element with a time greater than or equal to requested time
  // INFO: To delete all elements until (aka time less than) the requested time,
  // we set the read index to the found element. The element the read index
  // points to is kept. If the requested time is greater than the last valid
  // element (write_index-1) the function returns desc_end (= write_index) and
  // we set read_index to write_index to clear everything. If the requested time
  // is less than the first element the function returns desc_begin (=
  // read_index). Setting the read index to desc_begin would not harm but we do
  // nothing to reduce strain on the hardware.
  auto it = std::lower_bound(desc_begin, desc_end, time,
                             [](const fles::MicrosliceDescriptor& desc,
                                uint64_t t) { return desc.idx < t; });

  L_(trace) << "searching for time " << time << " in range "
            << desc_begin.get_index() << " - " << desc_end.get_index()
            << ". Setting read index to " << it.get_index();

  if (it != desc_begin) {
    m_cri_source_buffer->set_read_index(it.get_index());
  }
  return;
}

void* ComponentBuilder::alloc_buffer(size_t size_exp, size_t item_size) {
  size_t bytes = (UINT64_C(1) << size_exp) * item_size;
  L_(trace) << "allocating shm buffer of " << bytes << " bytes";
  return m_shm->allocate_aligned(bytes, sysconf(_SC_PAGESIZE));
}
