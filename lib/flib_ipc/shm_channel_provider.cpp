// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2015 Dirk Hutter

#include "shm_channel_provider.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <string>

template <typename T_DESC, typename T_DATA>
shm_channel_provider<T_DESC, T_DATA>::shm_channel_provider(
    ip::managed_shared_memory* shm,
    shm_device* shm_dev,
    size_t index,
    size_t data_buffer_size_exp,
    size_t desc_buffer_size_exp)
    : shm_dev_(shm_dev) {
  // allocate buffers
  void* data_buffer_raw = shm_alloc(shm, data_buffer_size_exp, sizeof(T_DATA));
  void* desc_buffer_raw = shm_alloc(shm, desc_buffer_size_exp, sizeof(T_DESC));

  // constuct channel exchange object in shared memory
  std::string channel_name = "shm_channel_" + std::to_string(index);
  shm_ch_ = shm->construct<shm_channel>(channel_name.c_str())(
      shm, data_buffer_raw, data_buffer_size_exp, sizeof(T_DATA),
      desc_buffer_raw, desc_buffer_size_exp, sizeof(T_DESC));
  set_write_index({0, 0});

  // initialize buffer info
  T_DATA* data_buffer = reinterpret_cast<T_DATA*>(data_buffer_raw);
  T_DESC* desc_buffer = reinterpret_cast<T_DESC*>(desc_buffer_raw);

  data_buffer_view_ = std::unique_ptr<RingBufferView<T_DATA>>(
      new RingBufferView<T_DATA>(data_buffer, data_buffer_size_exp));
  desc_buffer_view_ = std::unique_ptr<RingBufferView<T_DESC>>(
      new RingBufferView<T_DESC>(desc_buffer, desc_buffer_size_exp));
}

template <typename T_DESC, typename T_DATA>
DualIndex shm_channel_provider<T_DESC, T_DATA>::get_read_index() {
  ip::scoped_lock<ip::interprocess_mutex> lock(shm_dev_->m_mutex);
  shm_ch_->set_req_read_index(lock, false);
  return shm_ch_->read_index(lock);
}

template <typename T_DESC, typename T_DATA>
void shm_channel_provider<T_DESC, T_DATA>::set_write_index(
    DualIndex new_write_index) {
  TimedDualIndex write_index = {new_write_index, boost::posix_time::pos_infin};
  ip::scoped_lock<ip::interprocess_mutex> lock(shm_dev_->m_mutex);
  shm_ch_->set_req_write_index(lock, false);
  shm_ch_->set_write_index(lock, write_index);
}

template <typename T_DESC, typename T_DATA>
void shm_channel_provider<T_DESC, T_DATA>::set_eof(bool eof) {
  ip::scoped_lock<ip::interprocess_mutex> lock(shm_dev_->m_mutex);
  shm_ch_->set_eof(lock, eof);
}

template <typename T_DESC, typename T_DATA>
DualIndex shm_channel_provider<T_DESC, T_DATA>::get_occupied_size() {
  ip::scoped_lock<ip::interprocess_mutex> lock(shm_dev_->m_mutex);
  DualIndex read_index = shm_ch_->read_index(lock);
  DualIndex write_index = shm_ch_->write_index(lock).index;
  return write_index - read_index;
}

template class shm_channel_provider<fles::MicrosliceDescriptor, uint8_t>;
