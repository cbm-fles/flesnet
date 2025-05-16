// Copyright 2025 Dirk Hutter

#include "ComponentBuilder.hpp"
#include "cri.hpp"
#include "device_operator.hpp"
#include "log.hpp"
#include "parameters.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <chrono>
#include <csignal>
#include <memory>
#include <thread>

namespace ip = boost::interprocess;

using namespace std::chrono_literals;

namespace {
volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) { signal_status = sig; }

using T_DESC = fles::MicrosliceDescriptor;
using T_DATA = uint8_t;

int main(int argc, char* argv[]) {

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::string shm_id;

  try {

    parameters par(argc, argv);

    /////// Create Hardware Objects ///////////

    // create all needed CRI objects
    std::vector<std::unique_ptr<cri::cri_device>> cris;

    if (par.dev_autodetect()) {
      // use all available CRIs
      std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
      uint64_t num_dev = dev_op->device_count();
      L_(info) << "Total number of CRIs: " << num_dev << std::endl;

      for (size_t i = 0; i < num_dev; ++i) {
        cris.push_back(std::make_unique<cri::cri_device>(i));
        L_(info) << "Initialized CRI: " << cris.back()->print_devinfo()
                 << std::endl;
      }
    } else {
      // TODO: this should actually loop over a list of BDF, parameters need to
      // be implemented.
      cris.push_back(std::make_unique<cri::cri_device>(
          par.dev_addr().bus, par.dev_addr().dev, par.dev_addr().func));
      L_(info) << "Initialized CRI: " << cris.back()->print_devinfo()
               << std::endl;
    }

    // create all cri channels and remove inactive channels
    // TODO: may be replaced with explicit channel list
    std::vector<cri::cri_channel*> cri_channels;
    for (const auto& cri : cris) {
#ifdef __cpp_lib_containers_ranges
      cri_channels.append_range(cri->channels());
#else
      auto tmp = cri->channels();
      cri_channels.insert(cri_channels.end(), tmp.cbegin(), tmp.cend());
#endif
    }
    cri_channels.erase(std::remove_if(cri_channels.begin(), cri_channels.end(),
                                      [](decltype(cri_channels[0]) channel) {
                                        return channel->data_source() ==
                                               cri::cri_channel::rx_disable;
                                      }),
                       cri_channels.end());
    L_(info) << "Enabled cri channels detected: " << cri_channels.size();

    /////// Create Shared Memory //////////////

    // create shared memory segment with enough space for page aligned buffers
    // TODO: replace with explicit size for each channel
    size_t shm_size =
        ((UINT64_C(1) << par.data_buffer_size_exp()) * sizeof(T_DATA) +
         (UINT64_C(1) << par.desc_buffer_size_exp()) * sizeof(T_DESC) +
         2 * sysconf(_SC_PAGESIZE)) *
            cri_channels.size() +
        1000;

    shm_id = par.shm();
    auto shm = std::make_unique<ip::managed_shared_memory>(
        ip::create_only, shm_id.c_str(), shm_size);

    // Create Component Builder for each Channel
    std::vector<std::unique_ptr<ComponentBuilder>> builders;
    for (cri::cri_channel* channel : cri_channels) {
      builders.push_back(std::make_unique<ComponentBuilder>(
          shm.get(), channel, par.data_buffer_size_exp(),
          par.desc_buffer_size_exp()));
    }

    while (signal_status == 0) {
      for (auto&& builder : builders) {
        builder->proceed();
      }
      std::this_thread::sleep_for(100ms);
    }

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
    return EXIT_FAILURE;
  }

  // cleanup
  // shm smart pointer is out of scope here so the shared memory object is
  // already unmapped
  ip::shared_memory_object::remove(shm_id.c_str());

  L_(info) << "exiting";
  return EXIT_SUCCESS;
}
