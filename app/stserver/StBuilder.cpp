/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Dirk Hutter, Jan de Cuveland */

#include "StBuilder.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "Utility.hpp"
#include "device_operator.hpp"
#include "dma_channel.hpp"
#include "log.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <chrono>
#include <numeric>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace {
template <typename T>
inline std::span<T>
shm_allocate(boost::interprocess::managed_shared_memory* shm,
             std::size_t count) {
  std::size_t size_bytes = count * sizeof(T);
  void* buffer_raw = shm->allocate_aligned(size_bytes, sysconf(_SC_PAGESIZE));
  return {static_cast<T*>(buffer_raw), count};
}

} // namespace

StBuilder::StBuilder(volatile sig_atomic_t* signal_status,
                     cbm::Monitor* monitor,
                     SenderInfo sender_info,
                     StSender& st_sender,
                     bool device_autodetect,
                     pci_addr device_address,
                     std::string shm_id,
                     uint32_t pgen_channels,
                     int64_t pgen_microslice_duration_ns,
                     size_t pgen_microslice_size,
                     uint32_t pgen_flags,
                     int64_t timeslice_duration_ns,
                     int64_t timeout_ns,
                     size_t data_buffer_size,
                     size_t desc_buffer_size,
                     int64_t overlap_before_ns,
                     int64_t overlap_after_ns)
    : m_signal_status(signal_status), m_shm_id(std::move(shm_id)),
      m_timeslice_duration_ns(timeslice_duration_ns), m_timeout_ns(timeout_ns),
      m_overlap_before_ns(overlap_before_ns),
      m_overlap_after_ns(overlap_after_ns), m_monitor(monitor),
      m_sender_info(std::move(sender_info)), m_st_sender(st_sender) {
  /////// Create Hardware Objects ///////////

  try {
    // create all needed CRI objects
    if (device_autodetect) {
      // use all available CRIs
      std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
      uint64_t num_dev = dev_op->device_count();
      INFO("Total number of CRIs: {}", num_dev);

      for (size_t i = 0; i < num_dev; ++i) {
        m_cris.push_back(std::make_unique<cri::cri_device>(i));
        INFO("Initialized CRI: {}", m_cris.back()->print_devinfo());
      }
    } else {
      // TODO parameters: this should actually loop over a list of BDF addresses
      m_cris.push_back(std::make_unique<cri::cri_device>(
          device_address.bus, device_address.dev, device_address.func));
      INFO("Initialized CRI: {}", m_cris.back()->print_devinfo());
    }

    // create all cri channels and remove inactive channels
    // TODO parameters: to be replaced with explicit channel list
    for (const auto& cri : m_cris) {
#ifdef __cpp_lib_containers_ranges
      m_cri_channels.append_range(cri->channels());
#else
      auto tmp = cri->channels();
      m_cri_channels.insert(m_cri_channels.end(), tmp.cbegin(), tmp.cend());
#endif
    }
    m_cri_channels.erase(
        std::remove_if(m_cri_channels.begin(), m_cri_channels.end(),
                       [](decltype(m_cri_channels[0]) channel) {
                         return channel->data_source() ==
                                cri::cri_channel::rx_disable;
                       }),
        m_cri_channels.end());
    INFO("Enabled cri channels detected: {}", m_cri_channels.size());
  } catch (std::exception const& e) {
    WARN("Could not create hardware objects: {}", e.what());
  }

  /////// Create Shared Memory //////////////

  // create shared memory segment with enough space for page aligned buffers
  // TODO parameters: replace with explicit size for each channel
  size_t shm_size = (data_buffer_size * sizeof(uint8_t) +
                     desc_buffer_size * sizeof(fles::MicrosliceDescriptor) +
                     2 * sysconf(_SC_PAGESIZE)) *
                        (m_cri_channels.size() + pgen_channels) +
                    4096;

  INFO("Creating shared memory segment '{}' of size {}", m_shm_id,
       human_readable_count(shm_size, true));
  m_shm = std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, m_shm_id.c_str(), shm_size);

  // Create Channel objects for each CRI channel
  for (auto* cri_channel : m_cri_channels) {
    // set channel name, used for monitoring
    std::string channel_name = cri_channel->device_address() + "-" +
                               std::to_string(cri_channel->channel_index());

    // allocate buffers in shm
    std::span desc_buffer =
        shm_allocate<fles::MicrosliceDescriptor>(m_shm.get(), desc_buffer_size);
    std::span data_buffer =
        shm_allocate<uint8_t>(m_shm.get(), data_buffer_size);

    // initialize cri DMA engine
    cri_channel->init_dma(data_buffer.data(), data_buffer.size_bytes(),
                          desc_buffer.data(), desc_buffer.size_bytes());
    cri_channel->enable_readout();
    cri::basic_dma_channel* dma_channel = cri_channel->dma();

    m_channels.push_back(std::make_unique<Channel>(
        dma_channel, desc_buffer, data_buffer, overlap_before_ns,
        overlap_after_ns, channel_name));
  }

  // Create Channel objects for pattern generator channels if requested
  for (uint32_t i = 0; i < pgen_channels; ++i) {
    // set channel name, used for monitoring
    std::string channel_name = "pgen-" + std::to_string(i);

    // allocate buffers in shm
    std::span desc_buffer =
        shm_allocate<fles::MicrosliceDescriptor>(m_shm.get(), desc_buffer_size);
    std::span data_buffer =
        shm_allocate<uint8_t>(m_shm.get(), data_buffer_size);

    // initialize pgen
    m_pgen_channels.push_back(std::make_unique<cri::pgen_channel>(
        desc_buffer, data_buffer, i, pgen_microslice_duration_ns,
        pgen_microslice_size, pgen_flags));

    m_channels.push_back(std::make_unique<Channel>(
        m_pgen_channels.back().get(), desc_buffer, data_buffer,
        overlap_before_ns, overlap_after_ns, channel_name));
  }
}

void StBuilder::run() {
  for (auto&& channel : m_channels) {
    // ack far in the future to clear all elements
    channel->ack_before(2000000000000000000);
  }

  uint64_t ts_start_time = fles::system::current_time_ns() /
                           m_timeslice_duration_ns * m_timeslice_duration_ns;

  report_status();

  std::vector<Channel::State> states(m_channels.size());
  std::vector<std::size_t> ask_again(m_channels.size());
  std::iota(ask_again.begin(), ask_again.end(), 0);

  while (*m_signal_status == 0 && !m_st_sender.has_stopped()) {
    handle_completions();
    m_tasks.timer();

    // call check_component for all channels and store the states
    for (auto i : ask_again) {
      auto state = m_channels[i]->check_availability(ts_start_time,
                                                     m_timeslice_duration_ns);
      states[i] = state;
      if (state != Channel::State::TryLater) {
        // if the state is not TryLater, we do not need to ask again
        ask_again.erase(std::remove(ask_again.begin(), ask_again.end(), i),
                        ask_again.end());
      }
    }

    // if some channels are in the TryLater state and the timeout has not been
    // reached, wait a bit and try again
    bool timeout_reached = (fles::system::current_time_ns() >
                            ts_start_time + m_timeslice_duration_ns +
                                m_overlap_after_ns + m_timeout_ns);
    if (!ask_again.empty() && !timeout_reached) {
      std::this_thread::sleep_for(
          std::chrono::nanoseconds(m_timeslice_duration_ns / 10));
      continue;
    };

    // provide subtimeslice and advance to the next timeslice
    provide_subtimeslice(states, ts_start_time, m_timeslice_duration_ns);
    ts_start_time += m_timeslice_duration_ns;
    ask_again.resize(m_channels.size());
    std::iota(ask_again.begin(), ask_again.end(), 0);
  }
}

StBuilder::~StBuilder() {
  for (auto* cri_channel : m_cri_channels) {
    cri_channel->disable_readout();
  }
  m_channels.clear();
  m_pgen_channels.clear();
  m_cri_channels.clear();

  // cleanup
  INFO("Removing shared memory segment '{}'", m_shm_id);
  boost::interprocess::shared_memory_object::remove(m_shm_id.c_str());
}

void StBuilder::handle_completions() {
  while (auto id = m_st_sender.try_receive_completion()) {
    auto it = m_completed.find(*id);
    if (it != m_completed.end()) {
      it->second = true;
    } else {
      ERROR("{}| Received completion for unknown timeslice", *id);
    }
    auto iter = m_completed.begin();
    while (iter != m_completed.end() && iter->second) {
      ++iter;
    }
    if (iter != m_completed.begin()) {
      uint64_t last_completed = std::prev(iter)->first;
      for (auto&& channel : m_channels) {
        channel->ack_before((last_completed + 1) * m_timeslice_duration_ns);
      }
      m_completed.erase(m_completed.begin(), iter);
    }
  }
}

void StBuilder::provide_subtimeslice(std::vector<Channel::State> const& states,
                                     uint64_t start_time,
                                     uint64_t duration) {

  StHandle st;
  st.start_time_ns = start_time;
  st.duration_ns = duration;
  st.flags = 0;

  for (size_t i = 0; i < m_channels.size(); ++i) {
    auto& channel = m_channels[i];
    auto state = states[i];
    switch (state) {
    case Channel::State::Ok:
      st.components.push_back(channel->get_descriptor(start_time, duration));
      if (st.components.back().has_flag(TsComponentFlag::OverflowFlim)) {
        st.set_flag(TsFlag::OverflowFlim);
      }
      break;
    case Channel::State::Failed:
    case Channel::State::TryLater:
      st.set_flag(TsFlag::MissingComponents);
      break;
    }
  }

  // Announce the subtimeslice
  uint64_t ts_id = start_time / duration;
  m_st_sender.announce_subtimeslice(ts_id, st);
  m_completed[ts_id] = false;

  // Update statistics
  ++m_timeslice_count;
  m_component_count += st.components.size();
  for (const auto& comp : st.components) {
    m_microslice_count += comp.num_microslices;
    m_data_bytes += comp.ms_data_size();
  }
  if (st.has_flag(TsFlag::MissingComponents)) {
    ++m_timeslice_incomplete_count;
  }
}

void StBuilder::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  float max_buffer_utilization = 0.0;

  int64_t now_ns = fles::system::current_time_ns();
  for (const auto& channel : m_channels) {
    auto mon = channel->get_monitoring();
    max_buffer_utilization =
        std::max(max_buffer_utilization, std::max(mon.desc_buffer_utilization,
                                                  mon.data_buffer_utilization));
    if (m_monitor != nullptr) {
      if (mon.latest_microslice_time_ns) {
        int64_t delay = now_ns - mon.latest_microslice_time_ns.value();
        m_monitor->QueueMetric(
            "stserver_channel_status",
            {{"host", m_sender_info.address},
             {"port", std::to_string(m_sender_info.port)},
             {"channel", channel->name()}},
            {{"desc_buffer_utilization", mon.desc_buffer_utilization},
             {"data_buffer_utilization", mon.data_buffer_utilization},
             {"delay", delay}});
      } else {
        m_monitor->QueueMetric(
            "stserver_channel_status",
            {{"host", m_sender_info.address},
             {"port", std::to_string(m_sender_info.port)},
             {"channel", channel->name()}},
            {{"desc_buffer_utilization", mon.desc_buffer_utilization},
             {"data_buffer_utilization", mon.data_buffer_utilization}});
      }
    }
  }

  if (m_monitor != nullptr) {
    m_monitor->QueueMetric(
        "stserver_status",
        {{"host", m_sender_info.address},
         {"port", std::to_string(m_sender_info.port)}},
        {{"timeslice_count", m_timeslice_count},
         {"component_count", m_component_count},
         {"microslice_count", m_microslice_count},
         {"data_bytes", m_data_bytes},
         {"timeslice_incomplete_count", m_timeslice_incomplete_count},
         {"buffer_utilization", max_buffer_utilization}});
  }

  if (max_buffer_utilization > 0.9) {
    std::size_t completed_count =
        std::count_if(m_completed.begin(), m_completed.end(),
                      [](auto const& pair) { return pair.second; });
    std::size_t pending_count = m_completed.size() - completed_count;
    WARN("High buffer utilization ({:.1f}%), retracting {} pending "
         "subtimeslices",
         max_buffer_utilization * 100.0, pending_count);
    for (auto& [ts_id, completed] : m_completed) {
      if (!completed) {
        m_st_sender.retract_subtimeslice(ts_id);
      }
    }
    // TODO: Find a better solution
  }

  m_tasks.add([this] { report_status(); }, now + interval);
}
