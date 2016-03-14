// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2015 Dirk Hutter

#pragma once

#include "shm_channel_provider.hpp"
#include "shm_device.hpp"
#include "log.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <csignal>
#include <memory>
#include <string>

template <typename T_DESC, typename T_DATA> class shm_device_provider {

public:
  using shm_channel_provider_type = shm_channel_provider<T_DESC, T_DATA>;

  shm_device_provider(std::string shm_identifier,
                      size_t num_channels,
                      size_t data_buffer_size_exp,
                      size_t desc_buffer_size_exp)
      : shm_identifier_(shm_identifier) {
    shared_memory_object::remove(shm_identifier_.c_str());

    // create a big enough shared memory segment
    const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    size_t shm_size = ((UINT64_C(1) << data_buffer_size_exp) * sizeof(T_DATA) +
                       (UINT64_C(1) << desc_buffer_size_exp) * sizeof(T_DESC) +
                       2 * page_size + sizeof(shm_channel)) *
                          num_channels +
                      sizeof(shm_device) + 1000;
    shm_ = std::unique_ptr<managed_shared_memory>(new managed_shared_memory(
        create_only, shm_identifier.c_str(), shm_size));

    // create device exchange object in shared memory
    std::string device_name = "shm_device";
    shm_dev_ = shm_->construct<shm_device>(device_name.c_str())();

    // create channels
    for (size_t i = 0; i < num_channels; ++i) {
      shm_ch_vec_.push_back(std::unique_ptr<shm_channel_provider_type>(
          new shm_channel_provider_type(shm_.get(), shm_dev_, i,
                                        data_buffer_size_exp,
                                        desc_buffer_size_exp)));
      shm_dev_->inc_num_channels();
    }
  }

  ~shm_device_provider() {
    shared_memory_object::remove(shm_identifier_.c_str());
  }

  std::vector<shm_channel_provider_type*> channels() {
    std::vector<shm_channel_provider_type*> channels;
    for (auto& ch : shm_ch_vec_) {
      channels.push_back(ch.get());
    }
    return channels;
  }

  size_t num_channels() { return shm_ch_vec_.size(); }

private:
  std::string shm_identifier_;
  std::unique_ptr<managed_shared_memory> shm_;
  shm_device* shm_dev_;
  std::vector<std::unique_ptr<shm_channel_provider_type>> shm_ch_vec_;
};

using flib_shm_device_provider =
    shm_device_provider<fles::MicrosliceDescriptor, uint8_t>;
