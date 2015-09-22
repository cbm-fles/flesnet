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

  void set_read_index(DualIndex read_index) {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->set_read_index(lock, read_index);
    m_shm_ch->set_req_read_index(lock, true);
    m_shm_dev->m_cond_req.notify_one();
  }

  void update_write_index() {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->set_req_write_index(lock, true);
    m_shm_dev->m_cond_req.notify_one();
  }

  // get cached write_index
  TimedDualIndex get_write_index() {
    // TODO could be a shared lock
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    return m_shm_ch->write_index(lock);
  }

  // get latest write_index (blocking)
  std::pair<TimedDualIndex, bool>
  get_write_index_latest(const boost::posix_time::ptime& abs_timeout) {
    scoped_lock<interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->set_req_write_index(lock, true);
    m_shm_dev->m_cond_req.notify_one();
    bool ret = m_shm_ch->m_cond_write_index.timed_wait(lock, abs_timeout);
    TimedDualIndex write_index = m_shm_ch->write_index(lock);
    return std::make_pair(write_index, ret);
  }

  // get write_index newer than given relative timepoint (blocking)
  std::pair<TimedDualIndex, bool> get_write_index_newer_than(
      const boost::posix_time::time_duration& rel_time,
      const boost::posix_time::time_duration& rel_timeout =
          boost::posix_time::milliseconds(100)) {
    assert(!rel_time.is_negative());
    auto const now = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::ptime const abs_time = now - rel_time;
    boost::posix_time::ptime const abs_timeout = now + rel_timeout;

    std::pair<TimedDualIndex, bool> ret(get_write_index(), true);
    if (ret.first.updated < abs_time) {
      ret = get_write_index_latest(abs_timeout);
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
