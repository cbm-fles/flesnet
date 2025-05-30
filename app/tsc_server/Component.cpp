// Copyright 2025 Dirk Hutter

#include "Component.hpp"
#include "DualRingBuffer.hpp"
#include "log.hpp"

Component::Component(boost::interprocess::managed_shared_memory* shm,
                     cri::cri_channel* cri_channel,
                     size_t data_buffer_size_exp,
                     size_t desc_buffer_size_exp,
                     uint64_t overlap_before_ns,
                     uint64_t overlap_after_ns)
    : m_shm(shm), m_cri_channel(cri_channel),
      m_overlap_before_ns(overlap_before_ns),
      m_overlap_after_ns(overlap_after_ns) {

  // allocate buffers in shm
  void* data_buffer_raw = alloc_buffer(data_buffer_size_exp, sizeof(uint8_t));
  void* desc_buffer_raw =
      alloc_buffer(desc_buffer_size_exp, sizeof(fles::MicrosliceDescriptor));

  // initialize cri DMA engine
  m_cri_channel->init_dma(data_buffer_raw, data_buffer_size_exp + 0,
                          desc_buffer_raw, desc_buffer_size_exp + 5);
  m_cri_channel->enable_readout();

  // initialize buffer interface
  m_channel_interface = std::make_unique<ChannelInterface>(
      reinterpret_cast<fles::MicrosliceDescriptor*>(desc_buffer_raw),
      reinterpret_cast<uint8_t*>(data_buffer_raw), desc_buffer_size_exp,
      data_buffer_size_exp, m_cri_channel->dma());
}

Component::~Component() {
  m_cri_channel->disable_readout();
  m_cri_channel->deinit_dma();
  // TODO deallocate buffers if it is worth to do
}

void Component::ack_before(uint64_t time) {
  // fetch the current read and write index and initialize iterators
  // INFO: iterator desc_end points (as the write index does) to the next
  // element after the last valid element and must not be dereferenced
  // TODO: we may want to introduce a cached write index as member of component
  // and rely on other call to update it from HW
  DualIndex write_index = m_channel_interface->get_write_index();
  DualIndex read_index = m_channel_interface->get_read_index();
  auto desc_begin =
      m_channel_interface->desc_buffer().get_iter(read_index.desc);
  auto desc_end = m_channel_interface->desc_buffer().get_iter(write_index.desc);

  // Find the first element with a time greater than requested time and deduce 1
  // to get the last element with a time <= requested time. This has to be the
  // same logic as in find_component. We also deduce the overlap before time to
  // ensure next component can still be built.

  // INFO: To delete all elements until (aka time less than) the requested time,
  // we set the read index to the found element. The element the read index
  // points to is kept. If the requested time is greater than the last valid
  // element (write_index-1) the function returns desc_end (= write_index) and
  // we set read_index to write_index to clear everything. If the requested time
  // is less than the first element the function returns desc_begin (=
  // read_index). Setting the read index to desc_begin would not harm but we do
  // nothing to reduce strain on the hardware.
  time -= m_overlap_before_ns;
  auto it =
      std::upper_bound(desc_begin, desc_end, time,
                       [](uint64_t t, const fles::MicrosliceDescriptor& desc) {
                         return t < desc.idx;
                       });

  if (it != desc_begin) {
    it--;
    m_channel_interface->set_read_index(it.get_index());
  }

  L_(trace) << "searching for time " << time << " in range "
            << desc_begin.get_index() << " - " << desc_end.get_index()
            << ". Setting read index to " << it.get_index();

  return;
}

Component::State Component::check_availability(uint64_t start_time,
                                               uint64_t duration) {

  DualIndex write_index = m_channel_interface->get_write_index();
  DualIndex read_index = m_channel_interface->get_read_index();

  uint64_t first_ms_time = start_time - m_overlap_before_ns;
  uint64_t last_ms_time = start_time + duration + m_overlap_after_ns;

  L_(trace) << "searching for component [" << first_ms_time << ", "
            << last_ms_time << ").";

  if (write_index == read_index) {
    L_(trace) << "write and read index are equal, no data available";
    return Component::State::TryLater;
  }
  if (first_ms_time <
      m_channel_interface->desc_buffer().at(read_index.desc).idx) {
    L_(trace) << "too old, first time " << first_ms_time
              << " is before the first element in the buffer "
              << m_channel_interface->desc_buffer().at(read_index.desc).idx;
    return Component::State::Failed;
  }
  if (last_ms_time >=
      m_channel_interface->desc_buffer().at(write_index.desc - 1).idx) {
    L_(trace)
        << "not available yet, last time " << last_ms_time
        << " is after the last element in the buffer "
        << m_channel_interface->desc_buffer().at(write_index.desc - 1).idx;
    return Component::State::TryLater;
  }
  return Component::State::Ok;
}

fles::SubTimesliceComponentDescriptor
Component::get_descriptor(uint64_t start_time, uint64_t duration) {
  // find the component in the buffer
  auto [desc_begin_idx, desc_end_idx] = find_component(start_time, duration);
  assert(desc_begin_idx < desc_end_idx);

  // generate one or two i/o vectors for the descriptors
  std::vector<fles::ShmIovec> descriptors;
  auto& desc_buffer = m_channel_interface->desc_buffer();
  auto* desc_begin = &desc_buffer.at(desc_begin_idx);
  auto* desc_end = &desc_buffer.at(desc_end_idx);
  if (desc_begin < desc_end) {
    // the descriptors are contiguous in memory, so we only create one iovec
    descriptors.push_back(
        {m_shm->get_handle_from_address(desc_begin),
         (desc_end - desc_begin) * sizeof(fles::MicrosliceDescriptor)});
  } else if (desc_begin > desc_end) {
    // the descriptors have wrapped around the end of the buffer
    auto* desc_buffer_begin = desc_buffer.ptr();
    auto* desc_buffer_end = desc_buffer.ptr() + desc_buffer.size();
    descriptors.push_back(
        {m_shm->get_handle_from_address(desc_begin),
         (desc_buffer_end - desc_begin) * sizeof(fles::MicrosliceDescriptor)});
    descriptors.push_back(
        {m_shm->get_handle_from_address(desc_buffer_begin),
         (desc_end - desc_buffer_begin) * sizeof(fles::MicrosliceDescriptor)});
  }

  // find the data blocks for the component
  auto data_begin_idx = desc_begin->offset;
  auto data_end_idx = (desc_end - 1)->offset + (desc_end - 1)->size;

  // generate one or two i/o vectors for the descriptors
  std::vector<fles::ShmIovec> contents;
  auto& data_buffer = m_channel_interface->data_buffer();
  auto* data_begin = &data_buffer.at(data_begin_idx);
  auto* data_end = &data_buffer.at(data_end_idx);
  if (data_begin < data_end) {
    // the data blocks are contiguous in memory, so we only create one iovec
    contents.push_back({m_shm->get_handle_from_address(data_begin),
                        (data_end - data_begin) * sizeof(uint8_t)});
  } else if (data_begin > data_end) {
    // the data blocks have wrapped around the end of the buffer
    auto* data_buffer_begin = data_buffer.ptr();
    auto* data_buffer_end = data_buffer.ptr() + data_buffer.size();
    contents.push_back({m_shm->get_handle_from_address(data_begin),
                        (data_buffer_end - data_begin) * sizeof(uint8_t)});
    contents.push_back({m_shm->get_handle_from_address(data_buffer_begin),
                        (data_end - data_buffer_begin) * sizeof(uint8_t)});
  }

  // TODO: this is a placeholder for any aggregated information about the
  // microslices in the component
  bool is_missing_microslices = false;
  for (auto it = desc_buffer.get_iter(desc_begin_idx);
       it < desc_buffer.get_iter(desc_end_idx); ++it) {
    if ((it->flags &
         static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) != 0) {
      is_missing_microslices = true;
      break;
    }
  }

  return {descriptors, contents, is_missing_microslices};
}

// Returns the component's microslice descriptor indexes in the range
// [start_time - ovelap_before, start_time + duration + overlap_after). Expects
// that the component is available. Check the component state before calling
// find_component.
std::pair<uint64_t, uint64_t> Component::find_component(uint64_t start_time,
                                                        uint64_t duration) {
  DualIndex write_index = m_channel_interface->get_write_index();
  DualIndex read_index = m_channel_interface->get_read_index();

  uint64_t first_ms_time = start_time - m_overlap_before_ns;
  uint64_t last_ms_time = start_time + duration + m_overlap_after_ns;

  auto desc_begin =
      m_channel_interface->desc_buffer().get_iter(read_index.desc);
  auto desc_end = m_channel_interface->desc_buffer().get_iter(write_index.desc);

  // We search for microslice in the range [first_ms_time, last_ms_time)
  // (we use the index from the first search to limit the second search)

  // search for begin iterator, i.e., the microslice before the first microslice
  // > time
  auto first_it =
      std::upper_bound(desc_begin, desc_end, first_ms_time,
                       [](uint64_t t, const fles::MicrosliceDescriptor& desc) {
                         return t < desc.idx;
                       });
  if (first_it == desc_begin || first_it == desc_end) {
    throw std::out_of_range("Component::find_component: beginning of "
                            "component out of range");
  }
  uint64_t first_idx = first_it--.get_index();

  // search for the end iterator, i.e., the first microslice >= time
  auto last_it = std::lower_bound(first_it, desc_end, last_ms_time,
                                  [](const fles::MicrosliceDescriptor& desc,
                                     uint64_t t) { return desc.idx < t; });
  if (last_it == desc_begin || last_it == desc_end) {
    throw std::out_of_range(
        "Component::find_component: end of component out of range");
  }
  uint64_t last_idx = last_it.get_index();

  // TODO: technically we are not allowed to dereference last_it because its ms
  // is not guaranteed to be written
  L_(trace) << "Searching for component [" << first_ms_time << ", "
            << last_ms_time << "). Found component [" << first_it->idx << ", "
            << last_it->idx << ") with difference "
            << int64_t(first_it->idx - first_ms_time) << ", "
            << last_it->idx - last_ms_time << " as [" << first_idx << ", "
            << last_idx << "), " << last_idx - first_idx << " microslices";

  return {first_idx, last_idx};
}

void* Component::alloc_buffer(size_t size_exp, size_t item_size) {
  size_t bytes = (UINT64_C(1) << size_exp) * item_size;
  L_(trace) << "allocating shm buffer of " << bytes << " bytes";
  return m_shm->allocate_aligned(bytes, sysconf(_SC_PAGESIZE));
}
