// Copyright 2025 Dirk Hutter, Jan de Cuveland

#include "Channel.hpp"
#include "log.hpp"

namespace {
std::string pt(uint64_t time_ns) {
  // Chrono time_point from nanoseconds
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
      tp{std::chrono::nanoseconds{time_ns}};
  // Convert to time_t and remaining nanoseconds
  auto tp_seconds = std::chrono::time_point_cast<std::chrono::seconds>(tp);
  std::time_t t = std::chrono::system_clock::to_time_t(tp_seconds);
  auto rem_ns = (tp.time_since_epoch() - std::chrono::seconds{t}).count();

  std::tm tm = *std::localtime(&t);
  std::stringstream ss;
  ss.imbue(std::locale("")); // use locale for thousands separator
  ss << std::put_time(&tm, "%S") << "." << std::setfill('0') << std::setw(11)
     << rem_ns;
  return ss.str();
}
} // namespace

Channel::Channel(boost::interprocess::managed_shared_memory* shm,
                 cri::cri_channel* cri_channel,
                 size_t data_buffer_size,
                 size_t desc_buffer_size,
                 uint64_t overlap_before_ns,
                 uint64_t overlap_after_ns)
    : m_shm(shm), m_cri_channel(cri_channel),
      m_overlap_before_ns(overlap_before_ns),
      m_overlap_after_ns(overlap_after_ns) {

  std::size_t desc_buffer_size_bytes =
      desc_buffer_size * sizeof(fles::MicrosliceDescriptor);
  std::size_t data_buffer_size_bytes = data_buffer_size * sizeof(uint8_t);

  // allocate buffers in shm
  L_(trace) << "allocating shm buffers of " << data_buffer_size_bytes << "+"
            << desc_buffer_size_bytes << " bytes";
  void* data_buffer_raw =
      m_shm->allocate_aligned(data_buffer_size_bytes, sysconf(_SC_PAGESIZE));
  void* desc_buffer_raw =
      m_shm->allocate_aligned(desc_buffer_size_bytes, sysconf(_SC_PAGESIZE));

  // initialize cri DMA engine
  m_cri_channel->init_dma(data_buffer_raw, data_buffer_size_bytes,
                          desc_buffer_raw, desc_buffer_size_bytes);
  m_cri_channel->enable_readout();
  m_dma_channel = cri_channel->dma();

  // initialize buffer interface
  auto* desc_buffer =
      reinterpret_cast<fles::MicrosliceDescriptor*>(desc_buffer_raw);
  auto* data_buffer = reinterpret_cast<uint8_t*>(data_buffer_raw);
  m_desc_buffer =
      std::make_unique<RingBufferView<fles::MicrosliceDescriptor, false>>(
          desc_buffer, desc_buffer_size);
  m_data_buffer = std::make_unique<RingBufferView<uint8_t, false>>(
      data_buffer, data_buffer_size);

  // TODO: initialize m_read_index to the current hardware read index
}

Channel::~Channel() {
  m_cri_channel->disable_readout();
  m_cri_channel->deinit_dma();
  // TODO deallocate buffers if it is worth to do
}

void Channel::ack_before(uint64_t time) {
  // fetch the current read and write index and initialize iterators
  // INFO: iterator desc_end points (as the write index does) to the next
  // element after the last valid element and must not be dereferenced
  // TODO: we may want to introduce a cached write index as member of component
  // and rely on other call to update it from HW
  uint64_t write_index = m_dma_channel->get_desc_index();
  uint64_t read_index = m_cached_read_index;
  auto desc_begin = m_desc_buffer->get_iter(read_index);
  auto desc_end = m_desc_buffer->get_iter(write_index);

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
    set_read_index(it.get_index());
  }

  L_(trace) << "ack before: searching for time " << time << " in range "
            << desc_begin.get_index() << " - " << desc_end.get_index()
            << ". Setting read index to " << it.get_index();

  return;
}

Channel::State Channel::check_availability(uint64_t start_time,
                                           uint64_t duration) {

  uint64_t write_index = m_dma_channel->get_desc_index();
  uint64_t read_index = m_cached_read_index;

  uint64_t first_ms_time = start_time - m_overlap_before_ns;
  uint64_t last_ms_time = start_time + duration + m_overlap_after_ns;

  L_(trace) << "searching for component [" << pt(first_ms_time) << ", "
            << pt(last_ms_time) << ").";

  if (write_index == read_index) {
    L_(trace) << "write and read index are equal, no data available";
    return Channel::State::TryLater;
  }
  if (first_ms_time < m_desc_buffer->at(read_index).idx) {
    L_(trace) << "Failed; begin want= " << pt(first_ms_time)
              << " have=" << pt(m_desc_buffer->at(read_index).idx)
              << ", difference="
              << int64_t(m_desc_buffer->at(read_index).idx - first_ms_time);
    return Channel::State::Failed;
  }
  if (last_ms_time >= m_desc_buffer->at(write_index - 1).idx) {
    L_(trace) << "TryLater: end want=" << pt(last_ms_time)
              << " have=" << pt(m_desc_buffer->at(write_index - 1).idx)
              << ", difference="
              << int64_t(last_ms_time - m_desc_buffer->at(write_index - 1).idx);
    return Channel::State::TryLater;
  }
  return Channel::State::Ok;
}

fles::SubTimesliceComponentDescriptor
Channel::get_descriptor(uint64_t start_time, uint64_t duration) {
  // find the component in the buffer
  auto [desc_begin_idx, desc_end_idx] = find_component(start_time, duration);
  assert(desc_begin_idx < desc_end_idx);

  // generate one or two i/o vectors for the descriptors
  std::vector<fles::ShmIovec> descriptors;
  auto* desc_begin = &m_desc_buffer->at(desc_begin_idx);
  auto* desc_end = &m_desc_buffer->at(desc_end_idx);
  if (desc_begin < desc_end) {
    // the descriptors are contiguous in memory, so we only create one iovec
    descriptors.push_back(
        {m_shm->get_handle_from_address(desc_begin),
         (desc_end - desc_begin) * sizeof(fles::MicrosliceDescriptor)});
  } else if (desc_begin > desc_end) {
    // the descriptors have wrapped around the end of the buffer
    auto* desc_buffer_begin = m_desc_buffer->ptr();
    auto* desc_buffer_end = m_desc_buffer->ptr() + m_desc_buffer->size();
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
  auto* data_begin = &m_data_buffer->at(data_begin_idx);
  auto* data_end = &m_data_buffer->at(data_end_idx);
  if (data_begin < data_end) {
    // the data blocks are contiguous in memory, so we only create one iovec
    contents.push_back({m_shm->get_handle_from_address(data_begin),
                        (data_end - data_begin) * sizeof(uint8_t)});
  } else if (data_begin > data_end) {
    // the data blocks have wrapped around the end of the buffer
    auto* data_buffer_begin = m_data_buffer->ptr();
    auto* data_buffer_end = m_data_buffer->ptr() + m_data_buffer->size();
    contents.push_back({m_shm->get_handle_from_address(data_begin),
                        (data_buffer_end - data_begin) * sizeof(uint8_t)});
    contents.push_back({m_shm->get_handle_from_address(data_buffer_begin),
                        (data_end - data_buffer_begin) * sizeof(uint8_t)});
  }

  // TODO: this is a placeholder for any aggregated information about the
  // microslices in the component
  bool is_missing_microslices = false;
  for (auto it = m_desc_buffer->get_iter(desc_begin_idx);
       it < m_desc_buffer->get_iter(desc_end_idx); ++it) {
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
std::pair<uint64_t, uint64_t> Channel::find_component(uint64_t start_time,
                                                      uint64_t duration) {
  uint64_t write_index = m_dma_channel->get_desc_index();
  uint64_t read_index = m_cached_read_index;

  uint64_t first_ms_time = start_time - m_overlap_before_ns;
  uint64_t last_ms_time = start_time + duration + m_overlap_after_ns;

  auto desc_begin = m_desc_buffer->get_iter(read_index);
  auto desc_end = m_desc_buffer->get_iter(write_index);

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
  first_it--; // we want the first microslice <= time
  uint64_t first_idx = first_it.get_index();

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
  L_(trace) << "find_component: want [" << pt(first_ms_time) << ", "
            << pt(last_ms_time) << "), have [" << pt(first_it->idx) << ", "
            << pt(last_it->idx) << "), diff "
            << int64_t(first_it->idx - first_ms_time) << ", "
            << int64_t(last_it->idx - last_ms_time) << ", idx [" << first_idx
            << ", " << last_idx << "), " << last_idx - first_idx
            << " microslices";

  return {first_idx, last_idx};
}

void Channel::set_read_index(uint64_t read_index) {
  if (read_index == m_cached_read_index) {
    L_(trace) << "updating read_index, nothing to do for desc " << read_index;
    return;
  }

  if (read_index < m_cached_read_index) {
    std::stringstream ss;
    ss << "new read index desc " << read_index
       << " is smaller than the current read index desc " << m_cached_read_index
       << ", this should not happen!";
    throw std::runtime_error(ss.str());
  }

  uint64_t data_read_index = m_desc_buffer->at(read_index - 1).offset +
                             m_desc_buffer->at(read_index - 1).size;

  L_(trace) << "updating read_index: data " << data_read_index << " desc "
            << read_index;

  uint64_t desc_offset = m_desc_buffer->offset_bytes(read_index);
  uint64_t data_offset = m_data_buffer->offset_bytes(data_read_index);
  // Rount data_offset down to dma transfer size, will hang one transfer
  // size behind
  data_offset &= ~(m_dma_channel->dma_transfer_size() - 1);

  m_dma_channel->set_sw_read_pointers(data_offset, desc_offset);

  // cache the read_index locally
  m_cached_read_index = read_index;
}
