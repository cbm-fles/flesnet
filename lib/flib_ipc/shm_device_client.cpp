// Copyright 2015 Dirk Hutter

#include "shm_device_client.hpp"
#include "log.hpp"

template <typename T_DESC, typename T_DATA>
shm_device_client<T_DESC, T_DATA>::shm_device_client(
    const std::string& shm_identifier) {

  m_shm = std::unique_ptr<ip::managed_shared_memory>(
      new ip::managed_shared_memory(ip::open_only, shm_identifier.c_str()));

  // connect to global exchange object
  std::string device_name = "shm_device";
  m_shm_dev = m_shm->find<shm_device>(device_name.c_str()).first;
  if (m_shm_dev == nullptr) {
    throw std::runtime_error("Unable to find object" + device_name);
  }

  {
    ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
    if (!m_shm_dev->connect(lock)) {
      throw std::runtime_error("Server already in use");
    }
  }
}

template <typename T_DESC, typename T_DATA>
shm_device_client<T_DESC, T_DATA>::~shm_device_client() {
  try {
    ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_dev->disconnect(lock);
  } catch (ip::interprocess_exception const& e) {
    L_(error) << "Failed to disconnect device: " << e.what();
  }
}

template class shm_device_client<fles::MicrosliceDescriptor, uint8_t>;
