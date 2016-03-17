// Copyright 2015 Dirk Hutter

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <boost/interprocess/managed_shared_memory.hpp>

#include "MicrosliceDescriptor.hpp"
#include "shm_device.hpp"
#include "log.hpp"

using namespace boost::interprocess;

template <typename T_DESC, typename T_DATA> class shm_device_client {

public:

  shm_device_client(std::string shm_identifier) {

    m_shm = std::unique_ptr<managed_shared_memory>(
        new managed_shared_memory(open_only, shm_identifier.c_str()));

    // connect to global exchange object
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->find<shm_device>(device_name.c_str()).first;
    if (m_shm_dev == nullptr) {
      throw std::runtime_error("Unable to find object" + device_name);
    }

    {
      scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
      if (!m_shm_dev->connect(lock)) {
        throw std::runtime_error("Server already in use");
      }
    }
  }

  ~shm_device_client() {
    try {
      scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
      m_shm_dev->disconnect(lock);
    } catch (interprocess_exception const& e) {
      L_(error) << "Failed to disconnect device: " << e.what();
    }
  }

  size_t num_channels() { return m_shm_dev->num_channels(); }
  managed_shared_memory* shm() { return m_shm.get(); }

private:
  std::unique_ptr<managed_shared_memory> m_shm;
  shm_device* m_shm_dev = nullptr;
};

using flib_shm_device_client =
    shm_device_client<fles::MicrosliceDescriptor, uint8_t>;
