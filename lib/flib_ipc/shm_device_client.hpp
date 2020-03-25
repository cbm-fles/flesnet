// Copyright 2015 Dirk Hutter

#pragma once

#include "MicrosliceDescriptor.hpp"
#include "shm_device.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdint>
#include <memory>

namespace ip = boost::interprocess;

template <typename T_DESC, typename T_DATA> class shm_device_client {

public:
  explicit shm_device_client(const std::string& shm_identifier);

  ~shm_device_client();

  size_t num_channels() { return m_shm_dev->num_channels(); }
  ip::managed_shared_memory* shm() { return m_shm.get(); }

private:
  std::unique_ptr<ip::managed_shared_memory> m_shm;
  shm_device* m_shm_dev = nullptr;
};

using flib_shm_device_client =
    shm_device_client<fles::MicrosliceDescriptor, uint8_t>;
