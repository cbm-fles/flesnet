
#include <cstdint>
#include <memory>
#include <string>
#include <iostream>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include "flib_device.hpp"
#include "flib_link.hpp"

#include "shm_channel_server.hpp"
#include "shm_device.hpp"

using namespace boost::interprocess;
using namespace flib;

class shm_device_server {

public:

  shm_device_server(size_t data_buffer_size_exp,
                    size_t desc_buffer_size_exp,
                    volatile std::sig_atomic_t* signal_status)
    : m_signal_status(signal_status)
  {
    // create flib device
    m_flib = std::unique_ptr<flib_device>(new flib_device(0));
    std::vector<flib_link*> flib_links = m_flib->links();
    size_t num_links = m_flib->number_of_links();
    // TODO: add flib configuration from parameters here !
    
    // create a big enough shared memory segment
    size_t shm_size =
      ((UINT64_C(1) << data_buffer_size_exp) +
       (UINT64_C(1) << desc_buffer_size_exp) +
       sizeof(shm_channel)) * num_links +
      sizeof(shm_device) + 1000;
    m_shm = std::unique_ptr<managed_shared_memory>(new managed_shared_memory(create_only, "flib_shared_memory", shm_size));
    
    // constuct device exchange object in sharde memory
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->construct<shm_device>(device_name.c_str()) ();

    // create channels for active flib links
    size_t idx = 0;
    for (flib_link* link : flib_links) {
      if (link->data_sel() != flib::flib_link::rx_disable) {
        m_shm_ch_vec.push_back(std::unique_ptr
                               <shm_channel_server>(new shm_channel_server(m_shm.get(),
                                                                           idx, link,
                                                                           data_buffer_size_exp,
                                                                           desc_buffer_size_exp)));
        ++idx;
        m_shm_dev->inc_num_channels();
      }
    }    
  }

  ~shm_device_server() {}

  void run() {
    if (!m_run) { // don't start twice
      m_run = true;
      m_flib->enable_mc_cnt(true);
      while (m_run) {
        // claim lock at start-up
        scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
        
        // check nothing is todo everytime before sleeping
        // INFO: need to loop over all individual requests,
        // alternative would be global request cue.
        bool pending_req = false;
        for (const std::unique_ptr<shm_channel_server>& shm_ch : m_shm_ch_vec) {
          pending_req |= shm_ch->check_pending_req(lock);
        }
        if (!pending_req) {
          m_shm_dev->m_cond_req.wait(lock);
        }
//        if (*m_signal_status != 0) {
//          std::cout << "stopping " << std::endl;;
//        }
        
        // try to handle requests
        for (const std::unique_ptr<shm_channel_server>& shm_ch : m_shm_ch_vec) {
          shm_ch->try_handle_req(lock);
        }
      }
    }
  }

  void stop() {
    m_run = false;
    m_flib->enable_mc_cnt(false);
    // notify self to wake up, handle last requests if any and terminate
    m_shm_dev->m_cond_req.notify_one();
  }

  
private:

  std::string print_shm_info() {
    std::stringstream ss;
    ss  << "SHM INFO" << std::endl
        << "sanity "  << m_shm->check_sanity() << std::endl
        << "size "    << m_shm->get_size() << std::endl
        << "free_mem " << m_shm->get_free_memory() << std::endl
        << "num named obj " << m_shm->get_num_named_objects() << std::endl
        << "num unique obj " << m_shm->get_num_unique_objects() << std::endl
        << "name dev obj" << managed_shared_memory::get_instance_name(m_shm_dev) << std::endl;
    return ss.str();
  }

  // Members
  
  //Remove shared memory on construction and destruction of class
  struct shm_remove
  {
    shm_remove() { shared_memory_object::remove("flib_shared_memory");}
    ~shm_remove(){ shared_memory_object::remove("flib_shared_memory");}
  } remover;

  volatile std::sig_atomic_t* m_signal_status;
  std::unique_ptr<flib_device> m_flib;
  std::unique_ptr<managed_shared_memory> m_shm;
  shm_device* m_shm_dev = NULL;
  std::vector<std::unique_ptr<shm_channel_server>> m_shm_ch_vec;

  bool m_run = false;

};
