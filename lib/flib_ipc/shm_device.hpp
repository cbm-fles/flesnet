// Copyright 2015 Dirk Hutter

#pragma once

#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <cstdint>

namespace ip = boost::interprocess;

class shm_device {

public:
  shm_device() = default;

  ~shm_device() = default;

  void inc_num_channels() { ++m_num_channels; }

  void dec_num_channels() { --m_num_channels; }

  size_t num_channels() { return m_num_channels; }

  bool connect(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    ++m_clients;
    return true;
  }

  void disconnect(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    --m_clients;
  }

  // interprocess_mutex& mutex() { return m_mutex; }

  // interprocess_condition& cond_req() { return m_cond_req; }

  ip::interprocess_mutex m_mutex;
  ip::interprocess_condition m_cond_req;

private:
  size_t m_num_channels = 0;
  size_t m_clients = 0;
};
