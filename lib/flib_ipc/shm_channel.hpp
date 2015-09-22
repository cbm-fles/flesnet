// Copyright 2015 Dirk Hutter
// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#pragma once

#include <cstdint>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

using namespace boost::interprocess;

struct DualIndex {
  uint64_t desc;
  uint64_t data;
};

struct TimedDualIndex {
  DualIndex index;
  boost::posix_time::ptime updated;
};

class shm_channel {

public:
  shm_channel(managed_shared_memory* shm,
              void* data_buffer,
              size_t data_buffer_size_exp,
              void* desc_buffer,
              size_t desc_buffer_size_exp)
      : m_data_buffer_size_exp(data_buffer_size_exp),
        m_desc_buffer_size_exp(desc_buffer_size_exp) {
    set_buffer_handles(shm, data_buffer, desc_buffer);
  }

  ~shm_channel() {}

  void* data_buffer_ptr(managed_shared_memory* shm) const {
    return shm->get_address_from_handle(m_data_buffer_handle);
  }

  void* desc_buffer_ptr(managed_shared_memory* shm) const {
    return shm->get_address_from_handle(m_desc_buffer_handle);
  }

  size_t data_buffer_size_exp() { return m_data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return m_desc_buffer_size_exp; }

  // getter / setter
  bool req_ptr(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    return m_req_ptr;
  }

  bool req_offset(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    return m_req_offset;
  }

  void set_req_ptr(scoped_lock<interprocess_mutex>& lock, bool req) {
    assert(lock);
    m_req_ptr = req;
  }

  void set_req_offset(scoped_lock<interprocess_mutex>& lock, bool req) {
    assert(lock);
    m_req_offset = req;
  }

  TimedDualIndex offsets(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    TimedDualIndex offsets = m_offset;
    return offsets;
  }

  void set_offsets(scoped_lock<interprocess_mutex>& lock,
                   const TimedDualIndex offsets) {
    assert(lock);
    m_offset = offsets;
    m_cond_offsets.notify_all();
    return;
  }

  DualIndex ack_ptrs(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    DualIndex ptrs = m_read_ptr;
    return ptrs;
  }

  void set_ack_ptrs(scoped_lock<interprocess_mutex>& lock,
                    const DualIndex ack_ptrs) {
    assert(lock);
    m_read_ptr = ack_ptrs;
  }

  bool connect(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    if (m_clients != 0) {
      return false;
    } else {
      m_clients = 1;
      return true;
    }
  }

  void disconnect(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    m_clients = 0;
  }

  interprocess_condition m_cond_offsets;

private:
  void set_buffer_handles(managed_shared_memory* shm,
                          void* data_buffer,
                          void* desc_buffer) {
    m_data_buffer_handle = shm->get_handle_from_address(data_buffer);
    m_desc_buffer_handle = shm->get_handle_from_address(desc_buffer);
  }

  managed_shared_memory::handle_t m_data_buffer_handle;
  managed_shared_memory::handle_t m_desc_buffer_handle;
  size_t m_data_buffer_size_exp;
  size_t m_desc_buffer_size_exp;

  bool m_req_ptr = false;
  bool m_req_offset = false;

  DualIndex m_read_ptr{0, 0}; // INFO not actual hw value
  TimedDualIndex m_offset{{0, 0}, boost::posix_time::neg_infin};

  size_t m_clients = 0;
};
