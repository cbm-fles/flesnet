// Copyright 2015 Dirk Hutter

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <boost/interprocess/managed_shared_memory.hpp>

#include "shm_device.hpp"
#include "shm_channel_client.hpp"

using namespace boost::interprocess;

template <typename T_DESC, typename T_DATA> class shm_device_client {

public:
  using shm_channel_client_type = shm_channel_client<T_DESC, T_DATA>;

  shm_device_client(std::string shm_identifier) {

    m_shm = std::unique_ptr<managed_shared_memory>(
        new managed_shared_memory(open_only, shm_identifier.c_str()));

    // connect to global exchange object
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->find<shm_device>(device_name.c_str()).first;
    if (!m_shm_dev) {
      throw std::runtime_error("Unable to find object" + device_name);
    }

    {
      scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
      if (!m_shm_dev->connect(lock)) {
        throw std::runtime_error("Server already in use");
      }
    }

    size_t num_channels = m_shm_dev->num_channels();
    for (size_t i = 0; i < num_channels; ++i) {
      m_shm_ch_vec.push_back(std::unique_ptr<shm_channel_client_type>(
          new shm_channel_client_type(m_shm.get(), i)));
    }
  }

  ~shm_device_client() {
    // never disconnect to ensure server is only used once
  }

  std::vector<shm_channel_client_type*> channels() {
    std::vector<shm_channel_client_type*> channels;
    for (auto& l : m_shm_ch_vec) {
      channels.push_back(l.get());
    }
    return channels;
  }

  size_t num_channels() { return m_shm_ch_vec.size(); }

private:
  std::unique_ptr<managed_shared_memory> m_shm;
  shm_device* m_shm_dev = NULL;
  std::vector<std::unique_ptr<shm_channel_client_type>> m_shm_ch_vec;
};

using flib_shm_device_client =
    shm_device_client<fles::MicrosliceDescriptor, uint8_t>;
