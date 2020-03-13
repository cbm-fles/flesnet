// Copyright 2015 Dirk Hutter
// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#pragma once

#include "DualRingBuffer.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <cstdint>

namespace ip = boost::interprocess;

struct TimedDualIndex {
  DualIndex index;
  boost::posix_time::ptime updated;
};

class shm_channel {

public:
  shm_channel(ip::managed_shared_memory* shm,
              void* data_buffer,
              size_t data_buffer_size_exp,
              size_t data_item_size,
              void* desc_buffer,
              size_t desc_buffer_size_exp,
              size_t desc_item_size)
      : m_data_buffer_size_exp(data_buffer_size_exp),
        m_desc_buffer_size_exp(desc_buffer_size_exp),
        m_data_item_size(data_item_size), m_desc_item_size(desc_item_size) {
    set_buffer_handles(shm, data_buffer, desc_buffer);
  }

  ~shm_channel() = default;

  void* data_buffer_ptr(ip::managed_shared_memory* shm) const {
    return shm->get_address_from_handle(m_data_buffer_handle);
  }

  void* desc_buffer_ptr(ip::managed_shared_memory* shm) const {
    return shm->get_address_from_handle(m_desc_buffer_handle);
  }

  size_t data_buffer_size_exp() { return m_data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return m_desc_buffer_size_exp; }

  size_t data_item_size() { return m_data_item_size; }
  size_t desc_item_size() { return m_desc_item_size; }

  // getter / setter
  bool req_read_index(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    return m_req_read_index;
  }

  bool req_write_index(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    return m_req_write_index;
  }

  void set_req_read_index(ip::scoped_lock<ip::interprocess_mutex>& lock,
                          bool req) {
    assert(lock);
    m_req_read_index = req;
  }

  void set_req_write_index(ip::scoped_lock<ip::interprocess_mutex>& lock,
                           bool req) {
    assert(lock);
    m_req_write_index = req;
  }

  TimedDualIndex write_index(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    TimedDualIndex write_index = m_write_index;
    return write_index;
  }

  void set_write_index(ip::scoped_lock<ip::interprocess_mutex>& lock,
                       const TimedDualIndex write_index) {
    assert(lock);
    m_write_index = write_index;
    m_cond_write_index.notify_all();
  }

  DualIndex read_index(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    DualIndex read_index = m_read_index;
    return read_index;
  }

  void set_read_index(ip::scoped_lock<ip::interprocess_mutex>& lock,
                      const DualIndex read_index) {
    assert(lock);
    m_read_index = read_index;
  }

  bool eof(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    return m_eof;
  }

  void set_eof(ip::scoped_lock<ip::interprocess_mutex>& lock, bool eof) {
    assert(lock);
    m_eof = eof;
  }

  bool connect(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    if (m_clients != 0) {
      return false;
    }
    m_clients = 1;
    return true;
  }

  void disconnect(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock);
    m_clients = 0;
  }

  ip::interprocess_condition m_cond_write_index;

private:
  void set_buffer_handles(ip::managed_shared_memory* shm,
                          void* data_buffer,
                          void* desc_buffer) {
    m_data_buffer_handle = shm->get_handle_from_address(data_buffer);
    m_desc_buffer_handle = shm->get_handle_from_address(desc_buffer);
  }

  ip::managed_shared_memory::handle_t m_data_buffer_handle;
  ip::managed_shared_memory::handle_t m_desc_buffer_handle;
  size_t m_data_buffer_size_exp;
  size_t m_desc_buffer_size_exp;
  size_t m_data_item_size;
  size_t m_desc_item_size;

  bool m_req_read_index = false;
  bool m_req_write_index = false;

  DualIndex m_read_index{0, 0}; // INFO not actual hw value
  TimedDualIndex m_write_index{{0, 0}, boost::posix_time::neg_infin};

  bool m_eof = false;

  size_t m_clients = 0;
};
