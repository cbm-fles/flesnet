// Copyright 2015 Dirk Hutter

#pragma once

#include "etcd/Client.hpp"
#include "etcd/Watcher.hpp"
#include "flib_device.hpp"
#include "flib_link.hpp"
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

  shm_device_server(flib::flib_device* flib,
                    std::string shm_identifier,
                    size_t data_buffer_size_exp,
                    size_t desc_buffer_size_exp,
                    etcd_config_t etcd_config,
                    volatile std::sig_atomic_t* signal_status)
      : m_flib(flib), m_shm_identifier(std::move(shm_identifier)),
        m_etcd_config(std::move(etcd_config)), m_signal_status(signal_status) {

    if (m_etcd_config.use_etcd) {
      L_(info) << "Etcd URI: http://" << m_etcd_config.authority
               << m_etcd_config.path;
      m_etcd.reset(new etcd::Client("http://" + m_etcd_config.authority));
      m_signal_watcher = std::make_shared<etcd::Watcher>(
          "http://" + m_etcd_config.authority, m_etcd_config.path + "/running",
          [this](const etcd::Response& resp) {
            if (resp.is_ok() && resp.action() == "set" &&
                resp.value().as_string() == "0") {
              *m_signal_status = 1;
            } else {
              m_signal_watcher->renew_watch();
            }
          });
    }

    std::vector<flib::flib_link*> flib_links = m_flib->links();

    // delete deactivated links from vector
    flib_links.erase(
        std::remove_if(std::begin(flib_links), std::end(flib_links),
                       [](decltype(flib_links[0]) link) {
                         return link->data_sel() == flib::flib_link::rx_disable;
                       }),
        std::end(flib_links));
    L_(info) << "enabled flib links detected: " << flib_links.size();

    // create a big enough shared memory segment
    size_t shm_size = ((UINT64_C(1) << data_buffer_size_exp) * sizeof(T_DATA) +
                       (UINT64_C(1) << desc_buffer_size_exp) * sizeof(T_DESC) +
                       2 * sysconf(_SC_PAGESIZE) + sizeof(shm_channel)) *
                          flib_links.size() +
                      sizeof(shm_device) + 1000;
    m_shm = std::unique_ptr<ip::managed_shared_memory>(
        new ip::managed_shared_memory(ip::create_only, m_shm_identifier.c_str(),
                                      shm_size));

    // constuct device exchange object in sharde memory
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->construct<shm_device>(device_name.c_str())();

    // create channels for active flib links
    size_t idx = 0;
    for (flib::flib_link* link : flib_links) {
      m_shm_ch_vec.push_back(std::unique_ptr<shm_channel_server_type>(
          new shm_channel_server_type(m_shm.get(), m_shm_dev, idx, link,
                                      data_buffer_size_exp,
                                      desc_buffer_size_exp)));
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
      if (m_etcd) {
        m_etcd->set(m_etcd_config.path + "/running", "1").wait();
      }
      L_(info) << "flib server started and running";
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

  void stop() {
    if (m_etcd) {
      try {
        m_etcd->rm(m_etcd_config.path + "/running").wait();
      } catch (std::exception const& e) {
        L_(error) << "Exception: " << e.what();
      } catch (...) {
        L_(error) << "Exception: failed to remove etcd key.";
      }
    }
    m_run = false;
  }

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
  flib::flib_device* m_flib;
  std::string m_shm_identifier;
  etcd_config_t m_etcd_config;
  volatile std::sig_atomic_t* m_signal_status;
  std::unique_ptr<ip::managed_shared_memory> m_shm;
  shm_device* m_shm_dev = NULL;
  std::vector<std::unique_ptr<shm_channel_server_type>> m_shm_ch_vec;
  std::unique_ptr<etcd::Client> m_etcd;
  std::shared_ptr<etcd::Watcher> m_signal_watcher;

  bool m_run = false;
};

using flib_shm_device_server =
    shm_device_server<fles::MicrosliceDescriptor, uint8_t>;
