
#include <cstdint>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include "shm_channel.hpp"

using namespace boost::interprocess;

class shm_channel_client {

public:

  shm_channel_client(manageg_shared_memory* shm,
                     shm_dev* shm_dev,
                     size_t index)
    : m_shm(shm), m_shm_dev(shm_dev) {

    // connect to channel exchange object 
    std::string channel_name = "shm_channel_" +
      boost::lexical_cast<std::string>(index);
    m_shm_ch = m_shm->find<shm_channel>(channel_name.c_str()).first;
    if (!m_shm_ch) {
      throw FlibException("Unable to find object" + channel_name);
    }
    
    // initialize buffer pointers
    m_data_buffer = m_shm_ch->data_buffer_ptr(m_shm);
    m_desc_buffer = m_shm_ch->desc_buffer_ptr(m_shm);
  }
  
  ~shm_channel_client() {}

  data_buffer() { return m_data_buffer; }
  desc_buffer() { return m_desc_buffer; }
  
  set_ptrs(uint64_t data_ptr, uint64_t desc_ptr) {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->mutex());
    m_shm_ch->set_ptrs(lock, data_ptr, desc_ptr)
    m_shm_ch->set_req_ptr(lock, true);
    m_shm_dev->cond_req().notify_one();
  }

  update_offsets() {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->mutex());
    m_shm_ch->set_req_offset(lock, true);
    m_shm_dev->cond_req().notify_one();    
  }
  
  get_offsets(uint64_t& data_offset, uint64_t desc_offset) {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->mutex()); // TODO could be a shared lock
    m_shm_ch->offsets(lock, data_offset, desc_offset);
  }
  
private:

  managed_shared_memory* m_shm;
  shm_dev* m_shm_dev;
  
  shm_channel* m_shm_ch;
  void* m_data_buffer;
  void* m_desc_buffer;
  
};
