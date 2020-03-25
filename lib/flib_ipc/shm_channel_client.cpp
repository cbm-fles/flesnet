// Copyright 2015 Dirk Hutter
// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "shm_channel_client.hpp"
#include "log.hpp"
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <cassert>

template <typename T_DESC, typename T_DATA>
shm_channel_client<T_DESC, T_DATA>::shm_channel_client(
    const std::shared_ptr<flib_shm_device_client>& dev, size_t index)
    : m_dev(dev), m_shm(dev->shm()) {

  // connect to global exchange object
  std::string device_name = "shm_device";
  m_shm_dev = m_shm->find<shm_device>(device_name.c_str()).first;
  if (m_shm_dev == nullptr) {
    throw std::runtime_error("Unable to find object" + device_name);
  }

  // connect to channel exchange object
  std::string channel_name = "shm_channel_" + std::to_string(index);
  m_shm_ch = m_shm->find<shm_channel>(channel_name.c_str()).first;
  if (m_shm_ch == nullptr) {
    throw std::runtime_error("Unable to find object" + channel_name);
  }

  {
    ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
    if (!m_shm_ch->connect(lock)) {
      throw std::runtime_error("Channel " + channel_name + " already in use");
    }
  }

  if (m_shm_ch->desc_item_size() != sizeof(T_DESC) ||
      m_shm_ch->data_item_size() != sizeof(T_DATA)) {
    throw std::runtime_error("Channel " + channel_name + " is of wrong type");
  }

  // initialize buffer info
  m_data_buffer = m_shm_ch->data_buffer_ptr(m_shm);
  m_desc_buffer = m_shm_ch->desc_buffer_ptr(m_shm);
  m_data_buffer_size_exp = m_shm_ch->data_buffer_size_exp();
  m_desc_buffer_size_exp = m_shm_ch->desc_buffer_size_exp();

  T_DATA* data_buffer = reinterpret_cast<T_DATA*>(m_data_buffer);
  T_DESC* desc_buffer = reinterpret_cast<T_DESC*>(m_desc_buffer);

  data_buffer_view_ = std::unique_ptr<RingBufferView<T_DATA>>(
      new RingBufferView<T_DATA>(data_buffer, m_data_buffer_size_exp));
  desc_buffer_view_ = std::unique_ptr<RingBufferView<T_DESC>>(
      new RingBufferView<T_DESC>(desc_buffer, m_desc_buffer_size_exp));
}

template <typename T_DESC, typename T_DATA>
shm_channel_client<T_DESC, T_DATA>::~shm_channel_client() {
  try {
    ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
    m_shm_ch->disconnect(lock);
  } catch (ip::interprocess_exception const& e) {
    L_(error) << "Failed to disconnect client: " << e.what();
  }
}

template <typename T_DESC, typename T_DATA>
void shm_channel_client<T_DESC, T_DATA>::set_read_index(DualIndex read_index) {
  ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
  m_shm_ch->set_read_index(lock, read_index);
  m_shm_ch->set_req_read_index(lock, true);
  m_shm_dev->m_cond_req.notify_one();
}

template <typename T_DESC, typename T_DATA>
DualIndex shm_channel_client<T_DESC, T_DATA>::get_read_index() {
  ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
  return m_shm_ch->read_index(lock);
}

template <typename T_DESC, typename T_DATA>
void shm_channel_client<T_DESC, T_DATA>::update_write_index() {
  ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
  m_shm_ch->set_req_write_index(lock, true);
  m_shm_dev->m_cond_req.notify_one();
}

// get cached write_index
template <typename T_DESC, typename T_DATA>
TimedDualIndex shm_channel_client<T_DESC, T_DATA>::get_write_index_cached() {
  // TODO(Dirk): could be a shared lock
  ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
  return m_shm_ch->write_index(lock);
}

// get latest write_index (blocking)
template <typename T_DESC, typename T_DATA>
std::pair<TimedDualIndex, bool>
shm_channel_client<T_DESC, T_DATA>::get_write_index_latest(
    const boost::posix_time::ptime& abs_timeout) {
  ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
  m_shm_ch->set_req_write_index(lock, true);
  m_shm_dev->m_cond_req.notify_one();
  bool ret = m_shm_ch->m_cond_write_index.timed_wait(lock, abs_timeout);
  TimedDualIndex write_index = m_shm_ch->write_index(lock);
  return std::make_pair(write_index, ret);
}

// get write_index newer than given relative timepoint (blocking)
template <typename T_DESC, typename T_DATA>
std::pair<TimedDualIndex, bool>
shm_channel_client<T_DESC, T_DATA>::get_write_index_newer_than(
    const boost::posix_time::time_duration& rel_time,
    const boost::posix_time::time_duration& rel_timeout) {
  assert(!rel_time.is_negative());
  auto const now = boost::posix_time::microsec_clock::universal_time();
  boost::posix_time::ptime const abs_time = now - rel_time;
  boost::posix_time::ptime const abs_timeout = now + rel_timeout;

  std::pair<TimedDualIndex, bool> ret(get_write_index_cached(), true);
  if (ret.first.updated < abs_time) {
    ret = get_write_index_latest(abs_timeout);
  }
  assert(!(ret.second && (ret.first.updated < abs_time)));
  return ret;
}

template <typename T_DESC, typename T_DATA>
DualIndex shm_channel_client<T_DESC, T_DATA>::get_write_index() {
  return get_write_index_newer_than(boost::posix_time::microseconds(1))
      .first.index;
}

template <typename T_DESC, typename T_DATA>
bool shm_channel_client<T_DESC, T_DATA>::get_eof() {
  ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
  return m_shm_ch->eof(lock);
}

template class shm_channel_client<fles::MicrosliceDescriptor, uint8_t>;
