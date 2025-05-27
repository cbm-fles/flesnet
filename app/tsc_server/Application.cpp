// Copyright 2025 Dirk Hutter

#include "Application.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "device_operator.hpp"
#include "log.hpp"
#include <chrono>
#include <iostream>
#include <sys/types.h>
#include <thread>

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
      1000;

  shm_ = std::make_unique<ip::managed_shared_memory>(
      ip::create_only, par.shm_id().c_str(), shm_size);

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
  uint64_t ts_size_time = 100e6;

  // TODO: we may want to split this into startup and main loop phase
  while (signal_status_ == 0) {
    for (auto&& builder : builders_) {

      // search for a component
      auto state = builder->check_component_state(ts_start_time, ts_size_time);
      if (state == ComponentBuilder::ComponentState::Ok) {
        L_(info) << "Component available";
        try {
          builder->get_component(ts_start_time, ts_size_time);
        } catch (std::out_of_range const& e) {
          L_(error) << e.what();
        }
        ts_start_time += ts_size_time;
        builder->ack_before(ts_start_time);
        continue;
      }
      if (state == ComponentBuilder::ComponentState::TryLater) {
        L_(info) << "Component not available yet";
        // potentially we do not want to ack her all the time and split the
        // logic
        builder->ack_before(ts_start_time);
        // continue;
      }
      if (state == ComponentBuilder::ComponentState::Failed) {
        // if we are in the initializing phase this could be ok, in the real
        // phase this is an error hoewever we can argue that a maximum nagativ
        // microslice delay would guarantee component cant be too old at
        // startup if we add enough time to the start time for debug we just
        // skip 100 timeslices here
        ts_start_time += (ts_size_time * 10);
        L_(info) << "Component too old. setting new start time to "
                 << ts_start_time;
        builder->ack_before(ts_start_time);
        continue;
      }
      std::this_thread::sleep_for(200ms);
    }
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
