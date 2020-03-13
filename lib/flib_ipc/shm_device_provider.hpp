// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2015 Dirk Hutter

#pragma once

#include "shm_channel_provider.hpp"
#include "shm_device.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <memory>
#include <string>
#include <vector>

namespace ip = boost::interprocess;

template <typename T_DESC, typename T_DATA> class shm_device_provider {

public:
  using shm_channel_provider_type = shm_channel_provider<T_DESC, T_DATA>;

  shm_device_provider(const std::string& shm_identifier,
                      size_t num_channels,
                      size_t data_buffer_size_exp,
                      size_t desc_buffer_size_exp);

  ~shm_device_provider();

  std::vector<shm_channel_provider_type*> channels();

  size_t num_channels() { return shm_ch_vec_.size(); }

private:
  std::string shm_identifier_;
  std::unique_ptr<ip::managed_shared_memory> shm_;
  shm_device* shm_dev_;
  std::vector<std::unique_ptr<shm_channel_provider_type>> shm_ch_vec_;
};

using flib_shm_device_provider =
    shm_device_provider<fles::MicrosliceDescriptor, uint8_t>;
