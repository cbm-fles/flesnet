// Copyright 2025 Dirk Hutter

#include "ComponentBuilder.hpp"
#include "DualRingBuffer.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "log.hpp"

ComponentBuilder::ComponentBuilder(ip::managed_shared_memory* shm,
                                   cri::cri_channel* cri_channel,
                                   size_t data_buffer_size_exp,
                                   size_t desc_buffer_size_exp,
                                   uint64_t time_overlap_before,
                                   uint64_t time_overlap_after)
    : m_shm(shm), m_cri_channel(cri_channel),
      m_time_overlap_before(time_overlap_before),
      m_time_overlap_after(time_overlap_after) {

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
  // TODO: debugg functionality which can be removed, ack_before(uint64_max)
  // does the same
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

  // Find the first element with a time greater than requested time and deduce 1
  // to get the last element with a time <= requested time. This has to be the
  // same logic as in get_component. We also deduce the overlap before time to
  // ensure next component can still be built.

  // INFO: To delete all elements until (aka time less than) the requested time,
  // we set the read index to the found element. The element the read index
  // points to is kept. If the requested time is greater than the last valid
  // element (write_index-1) the function returns desc_end (= write_index) and
  // we set read_index to write_index to clear everything. If the requested time
  // is less than the first element the function returns desc_begin (=
  // read_index). Setting the read index to desc_begin would not harm but we do
  // nothing to reduce strain on the hardware.
  time -= m_time_overlap_before;
  auto it =
      std::upper_bound(desc_begin, desc_end, time,
                       [](uint64_t t, const fles::MicrosliceDescriptor& desc) {
                         return t < desc.idx;
                       });

  if (it != desc_begin) {
    it--;
    m_cri_source_buffer->set_read_index(it.get_index());
  }

  L_(trace) << "searching for time " << time << " in range "
            << desc_begin.get_index() << " - " << desc_end.get_index()
            << ". Setting read index to " << it.get_index();

  return;
}

ComponentBuilder::ComponentState
ComponentBuilder::check_component_state(uint64_t start_time,
                                        uint64_t duration) {

  DualIndex write_index = m_cri_source_buffer->get_write_index();
  DualIndex read_index = m_cri_source_buffer->get_read_index();

  uint64_t first_ms_time = start_time - m_time_overlap_before;
  uint64_t last_ms_time = start_time + duration + m_time_overlap_after;

  L_(trace) << "searching for component [" << first_ms_time << ", "
            << last_ms_time << ").";

  if (write_index == read_index) {
    L_(trace) << "write and read index are equal, no data available";
    return ComponentBuilder::ComponentState::TryLater;
  }
  if (first_ms_time <
      m_cri_source_buffer->desc_buffer().at(read_index.desc).idx) {
    L_(trace) << "too old, first time " << first_ms_time
              << " is before the first element in the buffer "
              << m_cri_source_buffer->desc_buffer().at(read_index.desc).idx;
    return ComponentBuilder::ComponentState::Failed;
  }
  if (last_ms_time >=
      m_cri_source_buffer->desc_buffer().at(write_index.desc - 1).idx) {
    L_(trace)
        << "not available yet, last time " << last_ms_time
        << " is after the last element in the buffer "
        << m_cri_source_buffer->desc_buffer().at(write_index.desc - 1).idx;
    return ComponentBuilder::ComponentState::TryLater;
  }
  return ComponentBuilder::ComponentState::Ok;
}

// Returns the component in the range [start_time - ovelap_before, start_time +
// duration + overlap_after) Expacts that the component is available. Check the
// compenten state before calling check_component_state.
void ComponentBuilder::get_component(uint64_t start_time, uint64_t duration) {
  DualIndex write_index = m_cri_source_buffer->get_write_index();
  DualIndex read_index = m_cri_source_buffer->get_read_index();

  uint64_t first_ms_time = start_time - m_time_overlap_before;
  uint64_t last_ms_time = start_time + duration + m_time_overlap_after;

  auto desc_begin =
      m_cri_source_buffer->desc_buffer().get_iter(read_index.desc);
  auto desc_end = m_cri_source_buffer->desc_buffer().get_iter(write_index.desc);

  // We search for mircroslice in the range [first_ms_time, last_ms_time)
  // (we use the index from the fist search to limit the second search)

  // search for begin iterator, i.e., the microslice before the first microslice
  // > time
  auto first_it =
      std::upper_bound(desc_begin, desc_end, first_ms_time,
                       [](uint64_t t, const fles::MicrosliceDescriptor& desc) {
                         return t < desc.idx;
                       });
  if (first_it == desc_begin || first_it == desc_end) {
    throw std::out_of_range(
        "ComponentBuilder::get_component: beginning of component out of range");
  }
  uint64_t firt_idx = first_it--.get_index();

  // search for the end iterator, i.e., the first microslice >= time
  auto last_it = std::lower_bound(first_it, desc_end, last_ms_time,
                                  [](const fles::MicrosliceDescriptor& desc,
                                     uint64_t t) { return desc.idx < t; });
  if (last_it == desc_begin || last_it == desc_end) {
    throw std::out_of_range(
        "ComponentBuilder::get_component: end of component ot of range");
  }
  uint64_t last_idx = last_it.get_index();

  // TODO: technically we are not allowed to dereference last_it because its ms
  // is not guaranteed to be written
  L_(trace) << "Searching for component [" << first_ms_time << ", "
            << last_ms_time << "). Found component [" << first_it->idx << ", "
            << last_it->idx << ") with difference "
            << int64_t(first_it->idx - first_ms_time) << ", "
            << last_it->idx - last_ms_time << " as [" << firt_idx << ", "
            << last_idx << "), " << last_idx - firt_idx << " microslices";
}

// private member functions

void* ComponentBuilder::alloc_buffer(size_t size_exp, size_t item_size) {
  size_t bytes = (UINT64_C(1) << size_exp) * item_size;
  L_(trace) << "allocating shm buffer of " << bytes << " bytes";
  return m_shm->allocate_aligned(bytes, sysconf(_SC_PAGESIZE));
}
