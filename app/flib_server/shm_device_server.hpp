// Copyright 2015 Dirk Hutter

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <iostream>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "log.hpp"
#include "flib_device.hpp"
#include "flib_link.hpp"

#include "shm_channel_server.hpp"
#include "shm_device.hpp"

using namespace boost::interprocess;
using namespace flib;

class shm_device_server {

public:

  shm_device_server(flib_device* flib,
                    size_t data_buffer_size_exp,
                    size_t desc_buffer_size_exp,
                    volatile std::sig_atomic_t* signal_status)
    : m_flib(flib), m_signal_status(signal_status)
  {
    std::vector<flib_link*> flib_links = m_flib->links();

    // delete deactivated links from vector
    flib_links.erase(std::remove_if(std::begin(flib_links), std::end(flib_links),
                                    [](decltype(flib_links[0]) link) {
                                      return link->data_sel() == flib::flib_link::rx_disable;
                                    }),
                     std::end(flib_links));
    L_(info) << "enabled flib links detected: " << flib_links.size();
    
    // create a big enough shared memory segment
    size_t shm_size =
      ((UINT64_C(1) << data_buffer_size_exp) +
       (UINT64_C(1) << desc_buffer_size_exp) +
       2*sysconf(_SC_PAGESIZE) +
       sizeof(shm_channel))
      * flib_links.size()
      + sizeof(shm_device) + 1000;
    m_shm = std::unique_ptr<managed_shared_memory>(new managed_shared_memory(create_only, "flib_shared_memory", shm_size));
    
    // constuct device exchange object in sharde memory
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->construct<shm_device>(device_name.c_str()) ();

    // create channels for active flib links
    size_t idx = 0;
    for (flib_link* link : flib_links) {
      m_shm_ch_vec.push_back(std::unique_ptr
                             <shm_channel_server>(new shm_channel_server(m_shm.get(),
                                                                         idx, link,
                                                                         data_buffer_size_exp,
                                                                         desc_buffer_size_exp)));
      ++idx;
      m_shm_dev->inc_num_channels();
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
        
        // check nothing is pending everytime before sleeping
        // INFO: need to loop over all individual requests,
        // alternative would be global request cue.
        bool pending_req = false;
        for (const std::unique_ptr<shm_channel_server>& shm_ch : m_shm_ch_vec) {
          pending_req |= shm_ch->check_pending_req(lock);
        }
        if (!pending_req) {
          // sleep if nothing is pending
          auto const abs_time = boost::posix_time::microsec_clock::universal_time()
            + boost::posix_time::milliseconds(100);
          m_shm_dev->m_cond_req.timed_wait(lock, abs_time);
        }
        if (*m_signal_status != 0) {
          stop();
        }
        
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

  flib_device* m_flib;
  volatile std::sig_atomic_t* m_signal_status;
  std::unique_ptr<managed_shared_memory> m_shm;
  shm_device* m_shm_dev = NULL;
  std::vector<std::unique_ptr<shm_channel_server>> m_shm_ch_vec;

  bool m_run = false;

};
