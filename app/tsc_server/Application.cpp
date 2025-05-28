// Copyright 2025 Dirk Hutter

#include "Application.hpp"
#include "ComponentBuilder.hpp"
#include "SubTimesliceDescriptor.hpp"
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
#include <vector>

namespace ip = boost::interprocess;

using namespace std::chrono_literals;

namespace {
inline uint64_t
chrono_to_timestamp(std::chrono::time_point<std::chrono::high_resolution_clock,
                                            std::chrono::nanoseconds> time) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             time.time_since_epoch())
      .count();
}

inline std::chrono::time_point<std::chrono::high_resolution_clock,
                               std::chrono::nanoseconds>
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
    // TODO: this should actually loop over a list of BDF, parameters need to
    // be implemented.
    cris_.push_back(std::make_unique<cri::cri_device>(
        par.device_address().bus, par.device_address().dev,
        par.device_address().func));
    L_(info) << "Initialized CRI: " << cris_.back()->print_devinfo()
             << std::endl;
  }

  // create all cri channels and remove inactive channels
  // TODO: may be replaced with explicit channel list
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
  // TODO: replace with explicit size for each channel
  size_t shm_size =
      ((UINT64_C(1) << par.data_buffer_size_exp()) * sizeof(uint8_t) +
       (UINT64_C(1) << par.desc_buffer_size_exp()) *
           sizeof(fles::MicrosliceDescriptor) +
       2 * sysconf(_SC_PAGESIZE)) *
          cri_channels_.size() +
      4096;

  shm_ = std::make_unique<ip::managed_shared_memory>(
      ip::create_only, par.shm_id().c_str(), shm_size);

  // Store a random UUID in the shared memory segment to identify it reliably
  shm_->construct<boost::uuids::uuid>(boost::interprocess::unique_instance)(
      shm_uuid_);

  // Create Component Builder for each Channel
  for (cri::cri_channel* channel : cri_channels_) {
    builders_.push_back(std::make_unique<ComponentBuilder>(
        shm_.get(), channel, par.data_buffer_size_exp(),
        par.desc_buffer_size_exp(), par.overlap_before_ns(),
        par.overlap_after_ns()));
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

  for (auto&& builder : builders_) {
    // ack far in the future to clear all elements
    builder->ack_before(2000000000000000000);
  }

  uint64_t ts_start_time =
      chrono_to_timestamp(std::chrono::high_resolution_clock::now()) + 1e9;
  uint64_t ts_size_time = par_.timeslice_duration_ns();

  std::vector<ComponentBuilder::ComponentState> states(builders_.size());
  std::vector<std::size_t> ask_again(builders_.size());
  std::iota(ask_again.begin(), ask_again.end(), 0);

  while (*signal_status_ == 0) {
    handle_completions();

    // call check_component for all builders and store the states
    for (auto i : ask_again) {
      auto state = builders_[i]->check_component(ts_start_time, ts_size_time);
      states[i] = state;
      if (state != ComponentBuilder::ComponentState::TryLater) {
        // if the state is not TryLater, we do not need to ask again
        ask_again.erase(std::remove(ask_again.begin(), ask_again.end(), i),
                        ask_again.end());
      }
    }

    // if some builders are in the TryLater state and the timeout has not been
    // reached, wait a bit and try again
    bool timeout_reached =
        (chrono_to_timestamp(std::chrono::high_resolution_clock::now()) >
         ts_start_time + par_.timeslice_duration_ns() +
             par_.timeslice_timeout_ns());
    if (!ask_again.empty() && !timeout_reached) {
      std::this_thread::sleep_for(10ms);
      continue;
    };

    // provide subtimeslice and advance to the next timeslice
    provide_subtimeslice(states, ts_start_time, ts_size_time);
    ts_start_time += ts_size_time;
    ask_again.resize(builders_.size());
    std::iota(ask_again.begin(), ask_again.end(), 0);
  }

  // Stop the item distributor thread
  item_distributor_->stop();
  distributor_thread.join();
}

Application::~Application() {
  // cleanup
  ip::shared_memory_object::remove(par_.shm_id().c_str());
  L_(info) << "Shared memory segment removed: " << par_.shm_id();
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
      for (auto&& builder : builders_) {
        builder->ack_before(acked_ * par_.timeslice_duration_ns());
      }
    } else {
      // we received a completion for an element other then the oldest,
      // we store it for later
      completions_.at(id) = id;
    }
  }
}

void Application::send_subtimeslice_item(fles::SubTimesliceDescriptor st) {
  // Serialize the SubTimesliceComponentDescriptor to a string
  std::ostringstream oss;
  {
    boost::archive::binary_oarchive oa(oss);
    oa << st;
  }
  // Send the serialized data as a work item
  uint64_t ts_id = st.start_time_ns / st.duration_ns;
  item_producer_->send_work_item(ts_id, oss.str());
}

void Application::provide_subtimeslice(
    std::vector<ComponentBuilder::ComponentState> const& states,
    uint64_t start_time,
    uint64_t duration) {

  // Create a SubTimesliceDescriptor
  fles::SubTimesliceDescriptor st;
  st.shm_identifier = par_.shm_id();
  st.shm_uuid = shm_uuid_;
  st.start_time_ns = start_time;
  st.duration_ns = duration;
  st.is_incomplete = false;

  // Create SubTimesliceComponentDescriptors for each builder
  for (size_t i = 0; i < builders_.size(); ++i) {
    auto& builder = builders_[i];
    auto state = states[i];
    switch (state) {
    case ComponentBuilder::ComponentState::Ok:
      st.components.push_back(builder->get_component(start_time, duration));
      break;
    case ComponentBuilder::ComponentState::Failed:
    case ComponentBuilder::ComponentState::TryLater:
      st.is_incomplete = true;
      break;
    }
  }

  // Send the SubTimesliceDescriptor as an item
  send_subtimeslice_item(st);
}