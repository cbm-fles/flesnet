// Copyright 2015 Dirk Hutter

#pragma once

#include <cstdint>
#include <cassert>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "log.hpp"

#include "shm_device.hpp"
#include "shm_channel.hpp"

using namespace boost::interprocess;

class shm_channel_client {

public:
  shm_channel_client(managed_shared_memory* shm, size_t index) : m_shm(shm) {

    // connect to global exchange object
    std::string device_name = "shm_device";
    m_shm_dev = m_shm->find<shm_device>(device_name.c_str()).first;
    if (!m_shm_dev) {
      throw std::runtime_error("Unable to find object" + device_name);
    }

    // connect to channel exchange object
    std::string channel_name =
        "shm_channel_" + boost::lexical_cast<std::string>(index);
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
    try {
      scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
      m_shm_ch->disconnect(lock);
    } catch (interprocess_exception const& e) {
      L_(error) << "Failed to disconnect client: " << e.what();
    }
  }

  void* data_buffer() { return m_data_buffer; }
  void* desc_buffer() { return m_desc_buffer; }
  size_t data_buffer_size_exp() { return m_data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return m_desc_buffer_size_exp; }

  void set_ack_ptrs(DualIndex ptrs) {
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

  // get cached offsets
  TimedDualIndex get_offsets() {
    // TODO could be a shared lock
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    return m_shm_ch->offsets(lock);
  }

  // get latest offsets (blocking)
  std::pair<TimedDualIndex, bool>
  get_offsets_latest(const boost::posix_time::ptime& abs_timeout) {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->set_req_offset(lock, true);
    m_shm_dev->m_cond_req.notify_one();
    bool ret = m_shm_ch->m_cond_offsets.timed_wait(lock, abs_timeout);
    TimedDualIndex offsets = m_shm_ch->offsets(lock);
    return std::make_pair(offsets, ret);
  }

  // get offsets newer than given relative timepoint (blocking)
  std::pair<TimedDualIndex, bool>
  get_offsets_newer_than(const boost::posix_time::time_duration& rel_time,
                         const boost::posix_time::time_duration& rel_timeout =
                             boost::posix_time::milliseconds(100)) {
    assert(!rel_time.is_negative());
    auto const now = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::ptime const abs_time = now - rel_time;
    boost::posix_time::ptime const abs_timeout = now + rel_timeout;

    std::pair<TimedDualIndex, bool> ret(get_offsets(), true);
    if (ret.first.updated < abs_time) {
      ret = get_offsets_latest(abs_timeout);
    }
    assert(!(ret.second && (ret.first.updated < abs_time)));
    return ret;
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
