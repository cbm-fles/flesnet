// Copyright 2025 Dirk Hutter, Jan de Cuveland

#include "Application.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "device_operator.hpp"
#include "dma_channel.hpp"
#include "log.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <chrono>
#include <iostream>
#include <numeric>
#include <span>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace {
template <typename T>
inline std::span<T>
shm_allocate(boost::interprocess::managed_shared_memory* shm,
             std::size_t count) {
  std::size_t size_bytes = count * sizeof(T);

  L_(trace) << "allocating shm buffer of " << size_bytes << " bytes";
  void* buffer_raw = shm->allocate_aligned(size_bytes, sysconf(_SC_PAGESIZE));

  return {static_cast<T*>(buffer_raw), count};
}

} // namespace

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status) {
  // start up monitoring
  if (!par.monitor_uri().empty()) {
    monitor_ = std::make_unique<cbm::Monitor>(par_.monitor_uri());
  }
  hostname_ = fles::system::current_hostname();

  /////// Create Hardware Objects ///////////

  try {
    // create all needed CRI objects
    if (par.device_autodetect()) {
      // use all available CRIs
      std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
      uint64_t num_dev = dev_op->device_count();
      L_(info) << "Total number of CRIs: " << num_dev << std::endl;

      for (size_t i = 0; i < num_dev; ++i) {
        cris_.push_back(std::make_unique<cri::cri_device>(i));
        L_(info) << "Initialized CRI: " << cris_.back()->print_devinfo()
                 << std::endl;
      }
    } else {
      // TODO parameters: this should actually loop over a list of BDF addresses
      cris_.push_back(std::make_unique<cri::cri_device>(
          par.device_address().bus, par.device_address().dev,
          par.device_address().func));
      L_(info) << "Initialized CRI: " << cris_.back()->print_devinfo()
               << std::endl;
    }

    // create all cri channels and remove inactive channels
    // TODO parameters: to be replaced with explicit channel list
    for (const auto& cri : cris_) {
#ifdef __cpp_lib_containers_ranges
      cri_channels.append_range(cri->channels());
#else
      auto tmp = cri->channels();
      cri_channels_.insert(cri_channels_.end(), tmp.cbegin(), tmp.cend());
#endif
    }
    cri_channels_.erase(std::remove_if(cri_channels_.begin(),
                                       cri_channels_.end(),
                                       [](decltype(cri_channels_[0]) channel) {
                                         return channel->data_source() ==
                                                cri::cri_channel::rx_disable;
                                       }),
                        cri_channels_.end());
    L_(info) << "Enabled cri channels detected: " << cri_channels_.size();
  } catch (std::exception const& e) {
    L_(warning) << "Could not create hardware objects: " << e.what();
  }

  /////// Create Shared Memory //////////////

  // create shared memory segment with enough space for page aligned buffers
  // TODO parameters: replace with explicit size for each channel
  size_t shm_size =
      (par.data_buffer_size() * sizeof(uint8_t) +
       par.desc_buffer_size() * sizeof(fles::MicrosliceDescriptor) +
       2 * sysconf(_SC_PAGESIZE)) *
          (cri_channels_.size() + par.pgen_channels()) +
      4096;

  shm_ = std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, par.shm_id().c_str(), shm_size);

  // Create Channel objects for each CRI channel
  for (auto* cri_channel : cri_channels_) {
    // set channel name, used for monitoring
    std::string channel_name = cri_channel->device_address() + "-" +
                               std::to_string(cri_channel->channel_index());

    // allocate buffers in shm
    std::span desc_buffer = shm_allocate<fles::MicrosliceDescriptor>(
        shm_.get(), par.desc_buffer_size());
    std::span data_buffer =
        shm_allocate<uint8_t>(shm_.get(), par.data_buffer_size());

    // initialize cri DMA engine
    cri_channel->init_dma(data_buffer.data(), data_buffer.size_bytes(),
                          desc_buffer.data(), desc_buffer.size_bytes());
    cri_channel->enable_readout();
    cri::basic_dma_channel* dma_channel = cri_channel->dma();

    channels_.push_back(std::make_unique<Channel>(
        dma_channel, desc_buffer, data_buffer, par.overlap_before_ns(),
        par.overlap_after_ns(), channel_name));
  }

  // Create Channel objects for pattern generator channels if requested
  for (uint32_t i = 0; i < par.pgen_channels(); ++i) {
    // set channel name, used for monitoring
    std::string channel_name = "pgen-" + std::to_string(i);

    // allocate buffers in shm
    std::span desc_buffer = shm_allocate<fles::MicrosliceDescriptor>(
        shm_.get(), par.desc_buffer_size());
    std::span data_buffer =
        shm_allocate<uint8_t>(shm_.get(), par.data_buffer_size());

    // initialize pgen
    pgen_channels_.push_back(std::make_unique<cri::pgen_channel>(
        desc_buffer, data_buffer, i, par.pgen_microslice_duration_ns(),
        par.pgen_microslice_size(), par.pgen_flags()));

    channels_.push_back(std::make_unique<Channel>(
        pgen_channels_.back().get(), desc_buffer, data_buffer,
        par.overlap_before_ns(), par.overlap_after_ns(), channel_name));
  }

  // Create StSender
  st_sender_ =
      std::make_unique<StSender>(par.tssched_address(), par.listen_port());
}

void Application::run() {
  for (auto&& channel : channels_) {
    // ack far in the future to clear all elements
    channel->ack_before(2000000000000000000);
  }

  uint64_t ts_start_time = fles::system::current_time_ns() /
                           par_.timeslice_duration_ns() *
                           par_.timeslice_duration_ns();
  acked_ = ts_start_time / par_.timeslice_duration_ns();

  report_status();

  std::vector<Channel::State> states(channels_.size());
  std::vector<std::size_t> ask_again(channels_.size());
  std::iota(ask_again.begin(), ask_again.end(), 0);

  while (*signal_status_ == 0) {
    handle_completions();
    scheduler_.timer();

    // call check_component for all channels and store the states
    for (auto i : ask_again) {
      auto state = channels_[i]->check_availability(
          ts_start_time, par_.timeslice_duration_ns());
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
                            ts_start_time + par_.timeslice_duration_ns() +
                                par_.overlap_after_ns() + par_.timeout_ns());
    if (!ask_again.empty() && !timeout_reached) {
      std::this_thread::sleep_for(
          std::chrono::nanoseconds(par_.timeslice_duration_ns() / 10));
      continue;
    };

    // provide subtimeslice and advance to the next timeslice
    provide_subtimeslice(states, ts_start_time, par_.timeslice_duration_ns());
    ts_start_time += par_.timeslice_duration_ns();
    ask_again.resize(channels_.size());
    std::iota(ask_again.begin(), ask_again.end(), 0);
  }
}

Application::~Application() {
  for (auto* cri_channel : cri_channels_) {
    cri_channel->disable_readout();
  }

  // cleanup
  boost::interprocess::shared_memory_object::remove(par_.shm_id().c_str());
  L_(info) << "Shared memory segment removed: " << par_.shm_id();

  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = std::chrono::milliseconds(200);
  std::this_thread::sleep_for(destruct_delay);
}

void Application::handle_completions() {
  while (auto id = st_sender_->try_receive_completion()) {
    if (*id == acked_) {
      // we reveived a completion for the oldest element in the buffer,
      // therefore we search for all consecutive elements ...
      do {
        ++acked_;
      } while (completions_.at(acked_) > *id);
      // ... and acknowledge them
      for (auto&& channel : channels_) {
        channel->ack_before(acked_ * par_.timeslice_duration_ns());
      }
    } else {
      // we received a completion for an element other then the oldest,
      // we store it for later
      completions_.at(*id) = *id;
    }
  }
}

void Application::provide_subtimeslice(
    std::vector<Channel::State> const& states,
    uint64_t start_time,
    uint64_t duration) {

  // Create a SubTimesliceDescriptor
  SubTimesliceHandle st;
  st.start_time_ns = start_time;
  st.duration_ns = duration;
  st.flags = 0;

  // Create SubTimesliceComponentDescriptors for each component
  for (size_t i = 0; i < channels_.size(); ++i) {
    auto& channel = channels_[i];
    auto state = states[i];
    switch (state) {
    case Channel::State::Ok:
      st.components.push_back(channel->get_descriptor(start_time, duration));
      if (st.components.back().has_flag(StComponentFlag::OverflowFlim)) {
        st.set_flag(StFlag::OverflowFlim);
      }
      break;
    case Channel::State::Failed:
    case Channel::State::TryLater:
      st.set_flag(StFlag::MissingComponents);
      break;
    }
  }

  // Announce the subtimeslice
  uint64_t ts_id = start_time / duration;
  st_sender_->announce_subtimeslice(ts_id, st);
  L_(trace) << "Sent announcement for timeslice " << ts_id << " with "
            << st.components.size() << " components (flags: " << st.flags
            << ")";

  // Update statistics
  ++timeslice_count_;
  component_count_ += st.components.size();
  for (const auto& comp : st.components) {
    microslice_count_ += comp.num_microslices();
    content_bytes_ += comp.contents_size();
    total_bytes_ += comp.descriptors_size() + comp.contents_size();
  }
  if (st.has_flag(StFlag::MissingComponents)) {
    ++timeslice_incomplete_count_;
  }
}

void Application::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (monitor_ != nullptr) {
    monitor_->QueueMetric(
        "tsc_server_status",
        {{"host", hostname_}, {"port", std::to_string(par_.listen_port())}},
        {{"timeslice_count", timeslice_count_},
         {"component_count", component_count_},
         {"microslice_count", microslice_count_},
         {"content_bytes", content_bytes_},
         {"timeslice_incomplete_count", timeslice_incomplete_count_}});
    int64_t now_ns = fles::system::current_time_ns();
    for (const auto& channel : channels_) {
      auto mon = channel->get_monitoring();
      if (mon.latest_microslice_time_ns) {
        int64_t delay = now_ns - mon.latest_microslice_time_ns.value();
        monitor_->QueueMetric(
            "tsc_server_channel_status",
            {{"host", hostname_},
             {"port", std::to_string(par_.listen_port())},
             {"channel", channel->name()}},
            {{"desc_buffer_utilization", mon.desc_buffer_utilization},
             {"data_buffer_utilization", mon.data_buffer_utilization},
             {"delay", delay}});
      } else {
        monitor_->QueueMetric(
            "tsc_server_channel_status",
            {{"host", hostname_},
             {"port", std::to_string(par_.listen_port())},
             {"channel", channel->name()}},
            {{"desc_buffer_utilization", mon.desc_buffer_utilization},
             {"data_buffer_utilization", mon.data_buffer_utilization}});
      }
    }
  }

  scheduler_.add([this] { report_status(); }, now + interval);
}