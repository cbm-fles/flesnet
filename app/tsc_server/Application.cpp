// Copyright 2025 Dirk Hutter, Jan de Cuveland

#include "Application.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "System.hpp"
#include "device_operator.hpp"
#include "log.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <iostream>
#include <numeric>
#include <sys/types.h>
#include <thread>

using namespace std::chrono_literals;

namespace {
[[maybe_unused]] inline uint64_t
chrono_to_timestamp(std::chrono::time_point<std::chrono::high_resolution_clock,
                                            std::chrono::nanoseconds> time) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             time.time_since_epoch())
      .count();
}

[[maybe_unused]] inline std::chrono::
    time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds>
    timestamp_to_chrono(uint64_t time) {
  return std::chrono::high_resolution_clock::time_point(
      std::chrono::nanoseconds(time));
}
} // namespace

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status) {
  boost::uuids::random_generator uuid_gen;
  shm_uuid_ = uuid_gen();

  // start up monitoring
  if (!par.monitor_uri().empty()) {
    monitor_ = std::make_unique<cbm::Monitor>(par_.monitor_uri());
  }
  hostname_ = fles::system::current_hostname();

  /////// Create Hardware Objects ///////////

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
  cri_channels_.erase(std::remove_if(cri_channels_.begin(), cri_channels_.end(),
                                     [](decltype(cri_channels_[0]) channel) {
                                       return channel->data_source() ==
                                              cri::cri_channel::rx_disable;
                                     }),
                      cri_channels_.end());
  L_(info) << "Enabled cri channels detected: " << cri_channels_.size();

  /////// Create Shared Memory //////////////

  // create shared memory segment with enough space for page aligned buffers
  // TODO parameters: replace with explicit size for each channel
  size_t shm_size =
      (par.data_buffer_size() * sizeof(uint8_t) +
       par.desc_buffer_size() * sizeof(fles::MicrosliceDescriptor) +
       2 * sysconf(_SC_PAGESIZE)) *
          cri_channels_.size() +
      4096;

  shm_ = std::make_unique<boost::interprocess::managed_shared_memory>(
      boost::interprocess::create_only, par.shm_id().c_str(), shm_size);

  // Store a random UUID in the shared memory segment to identify it reliably
  shm_->construct<boost::uuids::uuid>(boost::interprocess::unique_instance)(
      shm_uuid_);

  // Create Channel objects
  for (cri::cri_channel* channel : cri_channels_) {
    channels_.push_back(std::make_unique<Channel>(
        shm_.get(), channel, par.data_buffer_size(), par.desc_buffer_size(),
        par.overlap_before_ns(), par.overlap_after_ns()));
  }

  // Create ItemProducer and ItemDistributor
  const std::string producer_address = "inproc://" + par.shm_id();
  const std::string worker_address = "ipc://@" + par.shm_id();

  item_producer_ =
      std::make_unique<ItemProducer>(zmq_context_, producer_address);
  item_distributor_ = std::make_unique<ItemDistributor>(
      zmq_context_, producer_address, worker_address);
}

void Application::run() {
  std::thread distributor_thread(
      std::ref(*item_distributor_)); // Start the item distributor thread

  for (auto&& channel : channels_) {
    // ack far in the future to clear all elements
    channel->ack_before(2000000000000000000);
  }

  uint64_t ts_start_time =
      chrono_to_timestamp(std::chrono::high_resolution_clock::now()) /
      par_.timeslice_duration_ns() * par_.timeslice_duration_ns();
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
    bool timeout_reached =
        (chrono_to_timestamp(std::chrono::high_resolution_clock::now()) >
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

  // Stop the item distributor thread
  item_distributor_->stop();
  distributor_thread.join();
}

Application::~Application() {
  // cleanup
  boost::interprocess::shared_memory_object::remove(par_.shm_id().c_str());
  L_(info) << "Shared memory segment removed: " << par_.shm_id();

  // delay to allow monitor to process pending messages
  constexpr auto destruct_delay = std::chrono::milliseconds(200);
  std::this_thread::sleep_for(destruct_delay);
}

void Application::handle_completions() {
  ItemID id;
  while (item_producer_->try_receive_completion(&id)) {
    if (id == acked_) {
      // we reveived a completion for the oldest element in the buffer,
      // therefore we search for all consecutive elements ...
      do {
        ++acked_;
      } while (completions_.at(acked_) > id);
      // ... and acknowledge them
      for (auto&& channel : channels_) {
        channel->ack_before(acked_ * par_.timeslice_duration_ns());
      }
    } else {
      // we received a completion for an element other then the oldest,
      // we store it for later
      completions_.at(id) = id;
    }
  }
}

void Application::provide_subtimeslice(
    std::vector<Channel::State> const& states,
    uint64_t start_time,
    uint64_t duration) {

  // Create a SubTimesliceDescriptor
  fles::SubTimesliceDescriptor st;
  st.shm_identifier = par_.shm_id();
  st.shm_uuid = shm_uuid_;
  st.start_time_ns = start_time;
  st.duration_ns = duration;
  st.is_incomplete = false;

  // Create SubTimesliceComponentDescriptors for each component
  for (size_t i = 0; i < channels_.size(); ++i) {
    auto& channel = channels_[i];
    auto state = states[i];
    switch (state) {
    case Channel::State::Ok:
      st.components.push_back(channel->get_descriptor(start_time, duration));
      break;
    case Channel::State::Failed:
    case Channel::State::TryLater:
      st.is_incomplete = true;
      break;
    }
  }

  // Serialize the SubTimesliceDescriptor to a string
  std::ostringstream oss;
  {
    boost::archive::binary_oarchive oa(oss);
    oa << st;
  }

  // Send the serialized data as a work item
  uint64_t ts_id = start_time / duration;
  item_producer_->send_work_item(ts_id, oss.str());
  L_(trace) << "Sent SubTimesliceDescriptor for timeslice " << ts_id << " with "
            << st.components.size()
            << " components (complete: " << !st.is_incomplete << ")";

  // Update statistics
  ++timeslice_count_;
  component_count_ += st.components.size();
  for (const auto& comp : st.components) {
    microslice_count_ += comp.num_microslices();
    content_bytes_ += comp.contents_size();
    total_bytes_ += comp.descriptors_size() + comp.contents_size();
  }
  if (st.is_incomplete) {
    ++timeslice_incomplete_count_;
  }
}

void Application::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  int64_t now_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();

  if (monitor_ != nullptr) {
    monitor_->QueueMetric(
        "tsc_server_status", {{"host", hostname_}},
        {{"timeslice_count", timeslice_count_},
         {"component_count", component_count_},
         {"microslice_count", microslice_count_},
         {"content_bytes", content_bytes_},
         {"timeslice_incomplete_count", timeslice_incomplete_count_}});
    for (const auto& channel : channels_) {
      auto mon = channel->get_monitoring();
      int64_t delay = now_ns - mon.latest_microslice_time_ns;
      monitor_->QueueMetric(
          "tsc_server_channel_status",
          {{"host", hostname_}, {"channel", channel->name()}},
          {{"desc_buffer_utilization", mon.desc_buffer_utilization},
           {"data_buffer_utilization", mon.data_buffer_utilization},
           {"delay", delay}});
    }
  }

  scheduler_.add([this] { report_status(); }, now + interval);
}