
#include <cstdint>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/lexical_cast.hpp>

#include "shm_device.hpp"
#include "shm_channel.hpp"

using namespace boost::interprocess;

class shm_channel_client {

public:

  shm_channel_client(managed_shared_memory* shm,
                     size_t index)
    : m_shm(shm) {

    // connect to global exchange object
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->find<shm_device>(device_name.c_str()).first;
    if (!m_shm_dev) {
      throw std::runtime_error("Unable to find object" + device_name);
    }

    // connect to channel exchange object 
    std::string channel_name = "shm_channel_" +
      boost::lexical_cast<std::string>(index);
    m_shm_ch = m_shm->find<shm_channel>(channel_name.c_str()).first;
    if (!m_shm_ch) {
      throw std::runtime_error("Unable to find object" + channel_name);
    }

    {
      scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
      if (!m_shm_ch->connect(lock)) {
        throw std::runtime_error("Channel " + channel_name + " already in use");
      }
    }
    
    // initialize buffer info
    m_data_buffer = m_shm_ch->data_buffer_ptr(m_shm);
    m_desc_buffer = m_shm_ch->desc_buffer_ptr(m_shm);
    m_data_buffer_size_exp = m_shm_ch->data_buffer_size_exp();
    m_desc_buffer_size_exp = m_shm_ch->desc_buffer_size_exp();
  }
  
  ~shm_channel_client() {
    // TODO catch mutex exception
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->disconnect(lock);
  }

  void* data_buffer() { return m_data_buffer; }
  void* desc_buffer() { return m_desc_buffer; }
  size_t data_buffer_size_exp() { return m_data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return m_desc_buffer_size_exp; }

  
  void set_akc_ptrs(ack_ptrs_t ptrs) {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->set_ack_ptrs(lock, ptrs);
    m_shm_ch->set_req_ptr(lock, true);
    m_shm_dev->m_cond_req.notify_one();
  }

  void update_offsets() {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->set_req_offset(lock, true);
    m_shm_dev->m_cond_req.notify_one();    
  }
  
  offsets_t get_offsets() {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex); // TODO could be a shared lock
    return m_shm_ch->offsets(lock);
  }
  
private:

  managed_shared_memory* m_shm;
  shm_device* m_shm_dev;
  
  shm_channel* m_shm_ch;
  void* m_data_buffer;
  void* m_desc_buffer;
  size_t m_data_buffer_size_exp;
  size_t m_desc_buffer_size_exp;
  
};
