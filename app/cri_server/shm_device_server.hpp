// Copyright 2015 Dirk Hutter

#pragma once

#include "cri_channel.hpp"
#include "cri_device.hpp"
#include "log.hpp"
#include "shm_channel_server.hpp"
#include "shm_device.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace ip = boost::interprocess;

template <typename T_DESC, typename T_DATA> class shm_device_server {

public:
  using shm_channel_server_type = shm_channel_server<T_DESC, T_DATA>;

  shm_device_server(cri::cri_device* cri,
                    std::string shm_identifier,
                    size_t data_buffer_size_exp,
                    size_t desc_buffer_size_exp,
                    volatile std::sig_atomic_t* signal_status)
      : m_cri(cri), m_shm_identifier(std::move(shm_identifier)),
        m_signal_status(signal_status) {

    std::vector<cri::cri_channel*> cri_channels = m_cri->channels();

    // delete deactivated channels from vector
    cri_channels.erase(std::remove_if(std::begin(cri_channels),
                                      std::end(cri_channels),
                                      [](decltype(cri_channels[0]) channel) {
                                        return channel->data_source() ==
                                               cri::cri_channel::rx_disable;
                                      }),
                       std::end(cri_channels));
    L_(info) << "enabled cri channels detected: " << cri_channels.size();

    // create a big enough shared memory segment
    size_t shm_size = ((UINT64_C(1) << data_buffer_size_exp) * sizeof(T_DATA) +
                       (UINT64_C(1) << desc_buffer_size_exp) * sizeof(T_DESC) +
                       2 * sysconf(_SC_PAGESIZE) + sizeof(shm_channel)) *
                          cri_channels.size() +
                      sizeof(shm_device) + 1000;
    m_shm = std::make_unique<ip::managed_shared_memory>(
        ip::create_only, m_shm_identifier.c_str(), shm_size);

    // constuct device exchange object in sharde memory
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->construct<shm_device>(device_name.c_str())();

    // create channels for active cri channels
    size_t idx = 0;
    for (cri::cri_channel* channel : cri_channels) {
      m_shm_ch_vec.push_back(std::make_unique<shm_channel_server_type>(
          m_shm.get(), m_shm_dev, idx, channel, data_buffer_size_exp,
          desc_buffer_size_exp));
      ++idx;
      m_shm_dev->inc_num_channels();
    }
  }

  ~shm_device_server() {
    ip::shared_memory_object::remove(m_shm_identifier.c_str());
  }

  void run() {
    if (!m_run) { // don't start twice
      m_run = true;
      L_(info) << "cri server started and running";
      while (m_run) {
        // claim lock at start-up
        ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);

        // check nothing is pending everytime before sleeping
        // INFO: need to loop over all individual requests,
        // alternative would be global request cue.
        bool pending_req = false;
        for (const std::unique_ptr<shm_channel_server_type>& shm_ch :
             m_shm_ch_vec) {
          pending_req |= shm_ch->check_pending_req(lock);
        }
        if (!pending_req) {
          // sleep if nothing is pending
          auto const abs_time =
              boost::posix_time::microsec_clock::universal_time() +
              boost::posix_time::milliseconds(100);
          m_shm_dev->m_cond_req.timed_wait(lock, abs_time);
        }
        if (*m_signal_status != 0) {
          stop();
        }

        // try to handle requests
        for (const std::unique_ptr<shm_channel_server_type>& shm_ch :
             m_shm_ch_vec) {
          shm_ch->try_handle_req(lock);
        }
      }
    }
  }

  void stop() { m_run = false; }

private:
  std::string print_shm_info() {
    std::stringstream ss;
    ss << "SHM INFO" << std::endl
       << "sanity " << m_shm->check_sanity() << std::endl
       << "size " << m_shm->get_size() << std::endl
       << "free_mem " << m_shm->get_free_memory() << std::endl
       << "num named obj " << m_shm->get_num_named_objects() << std::endl
       << "num unique obj " << m_shm->get_num_unique_objects() << std::endl
       << "name dev obj"
       << ip::managed_shared_memory::get_instance_name(m_shm_dev) << std::endl;
    return ss.str();
  }

  // Members
  cri::cri_device* m_cri;
  std::string m_shm_identifier;
  volatile std::sig_atomic_t* m_signal_status;
  std::unique_ptr<ip::managed_shared_memory> m_shm;
  shm_device* m_shm_dev = nullptr;
  std::vector<std::unique_ptr<shm_channel_server_type>> m_shm_ch_vec;

  bool m_run = false;
};

using cri_shm_device_server =
    shm_device_server<fles::MicrosliceDescriptor, uint8_t>;
