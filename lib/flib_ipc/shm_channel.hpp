#pragma once

#include <cstdint>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

using namespace boost::interprocess;

typedef struct {
  uint64_t data_offset;
  uint64_t desc_offset;
} offsets_t;

typedef struct {
  uint64_t data_ptr;
  uint64_t desc_ptr;
} ack_ptrs_t;

class shm_channel {

public:

  shm_channel(managed_shared_memory* shm,
              void* data_buffer, size_t data_buffer_size_exp,
              void* desc_buffer, size_t desc_buffer_size_exp)
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
    
  offsets_t offsets(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    offsets_t offsets;
    offsets.data_offset = m_data_offset;
    offsets.desc_offset = m_desc_offset;
    return offsets;
  }

  void set_offsets(scoped_lock<interprocess_mutex>& lock,
                   const offsets_t offsets) {
    assert(lock);
    m_data_offset = offsets.data_offset;
    m_desc_offset = offsets.desc_offset;
    return;
  }

  ack_ptrs_t ack_ptrs(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    ack_ptrs_t ptrs;
    ptrs.data_ptr = m_data_read_ptr;
    ptrs.desc_ptr = m_desc_read_ptr;
    return ptrs;
  }
  
  void set_ack_ptrs(scoped_lock<interprocess_mutex>& lock,
                    const ack_ptrs_t ack_ptrs) {
    assert(lock);
    m_data_read_ptr = ack_ptrs.data_ptr;
    m_desc_read_ptr = ack_ptrs.desc_ptr;
    return;
  }

  bool connect(scoped_lock<interprocess_mutex>& lock) {
    assert(lock);
    if ( m_clients != 0 ) {
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
  
  uint64_t m_data_read_ptr = 0; // TODO initialize from hw
  uint64_t m_desc_read_ptr = 0;  
  uint64_t m_data_offset = 0;
  uint64_t m_desc_offset = 0;
  
  size_t m_clients = 0;
};
