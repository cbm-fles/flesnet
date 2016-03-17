// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2015 Dirk Hutter

#pragma once

#include "shm_channel.hpp"
#include "DualRingBuffer.hpp"
#include "log.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

namespace {
inline void*
shm_alloc(managed_shared_memory* shm, size_t size_exp, size_t item_size) {
  size_t bytes = (UINT64_C(1) << size_exp) * item_size;
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  L_(trace) << "allocating aligned shm buffer of " << bytes << " bytes";
  return shm->allocate_aligned(bytes, page_size);
}
} // namespace

template <typename T_DESC, typename T_DATA>
class shm_channel_provider
    : public DualRingBufferWriteInterface<T_DESC, T_DATA> {

public:
  shm_channel_provider(managed_shared_memory* shm,
                       shm_device* shm_dev,
                       size_t index,
                       size_t data_buffer_size_exp,
                       size_t desc_buffer_size_exp)
      : shm_dev_(shm_dev) {
    // allocate buffers
    void* data_buffer_raw =
        shm_alloc(shm, data_buffer_size_exp, sizeof(T_DATA));
    void* desc_buffer_raw =
        shm_alloc(shm, desc_buffer_size_exp, sizeof(T_DESC));

    // constuct channel exchange object in shared memory
    std::string channel_name =
        "shm_channel_" + boost::lexical_cast<std::string>(index);
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

  virtual DualIndex get_read_index() override {
    scoped_lock<interprocess_mutex> lock(shm_dev_->m_mutex);
    shm_ch_->set_req_read_index(lock, false);
    return shm_ch_->read_index(lock);
  };

  virtual void set_write_index(DualIndex new_write_index) override {
    TimedDualIndex write_index = {new_write_index,
                                  boost::posix_time::pos_infin};
    scoped_lock<interprocess_mutex> lock(shm_dev_->m_mutex);
    shm_ch_->set_req_write_index(lock, false);
    shm_ch_->set_write_index(lock, write_index);
  };

  virtual RingBufferView<T_DATA>& data_buffer() override {
    return *data_buffer_view_;
  }

  virtual RingBufferView<T_DESC>& desc_buffer() override {
    return *desc_buffer_view_;
  }

  DualIndex get_occupied_size() {
    scoped_lock<interprocess_mutex> lock(shm_dev_->m_mutex);
    DualIndex read_index = shm_ch_->read_index(lock);
    DualIndex write_index = shm_ch_->write_index(lock).index;
    return write_index - read_index;
  }

  bool empty() { return get_occupied_size() == DualIndex({0, 0}); }

private:
  shm_device* shm_dev_;
  shm_channel* shm_ch_;

  std::unique_ptr<RingBufferView<T_DATA>> data_buffer_view_;
  std::unique_ptr<RingBufferView<T_DESC>> desc_buffer_view_;
};
