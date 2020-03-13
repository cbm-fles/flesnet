// Copyright 2015 Dirk Hutter
// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#pragma once

#include "DualRingBuffer.hpp"
#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"
#include "shm_channel.hpp"
#include "shm_device.hpp"
#include "shm_device_client.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdint>
#include <memory>

namespace ip = boost::interprocess;

template <typename T_DESC, typename T_DATA>
class shm_channel_client : public DualRingBufferReadInterface<T_DESC, T_DATA> {

public:
  shm_channel_client(const std::shared_ptr<flib_shm_device_client>& dev,
                     size_t index);
  shm_channel_client(const shm_channel_client&) = delete;
  void operator=(const shm_channel_client&) = delete;

  ~shm_channel_client() override;

  void set_read_index(DualIndex read_index) override;

  DualIndex get_read_index() override;

  void update_write_index();

  // get cached write_index
  TimedDualIndex get_write_index_cached();

  // get latest write_index (blocking)
  std::pair<TimedDualIndex, bool>
  get_write_index_latest(const boost::posix_time::ptime& abs_timeout);

  // get write_index newer than given relative timepoint (blocking)
  std::pair<TimedDualIndex, bool> get_write_index_newer_than(
      const boost::posix_time::time_duration& rel_time,
      const boost::posix_time::time_duration& rel_timeout =
          boost::posix_time::milliseconds(100));

  DualIndex get_write_index() override;

  bool get_eof() override;

  size_t data_buffer_size_exp() { return m_data_buffer_size_exp; }
  size_t desc_buffer_size_exp() { return m_desc_buffer_size_exp; }

  RingBufferView<T_DATA>& data_buffer() override { return *data_buffer_view_; }
  RingBufferView<T_DESC>& desc_buffer() override { return *desc_buffer_view_; }

private:
  std::shared_ptr<flib_shm_device_client> m_dev;
  ip::managed_shared_memory* m_shm;
  shm_device* m_shm_dev;

  shm_channel* m_shm_ch;
  void* m_data_buffer;
  void* m_desc_buffer;
  size_t m_data_buffer_size_exp;
  size_t m_desc_buffer_size_exp;

  std::unique_ptr<RingBufferView<T_DATA>> data_buffer_view_;
  std::unique_ptr<RingBufferView<T_DESC>> desc_buffer_view_;
};

using flib_shm_channel_client =
    shm_channel_client<fles::MicrosliceDescriptor, uint8_t>;
