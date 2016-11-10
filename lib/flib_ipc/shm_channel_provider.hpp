// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2015 Dirk Hutter

#pragma once

#include "DualRingBuffer.hpp"
#include "log.hpp"
#include "shm_channel.hpp"
#include "shm_device.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <memory>

namespace ip = boost::interprocess;

namespace {
inline void*
shm_alloc(ip::managed_shared_memory* shm, size_t size_exp, size_t item_size) {
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
  shm_channel_provider(ip::managed_shared_memory* shm,
                       shm_device* shm_dev,
                       size_t index,
                       size_t data_buffer_size_exp,
                       size_t desc_buffer_size_exp);

  DualIndex get_read_index() override;

  void set_write_index(DualIndex new_write_index) override;

  void set_eof(bool eof) override;

  DualIndex get_occupied_size();

  bool empty() { return get_occupied_size() == DualIndex({0, 0}); }

  RingBufferView<T_DATA>& data_buffer() override { return *data_buffer_view_; }
  RingBufferView<T_DESC>& desc_buffer() override { return *desc_buffer_view_; }

private:
  shm_device* shm_dev_;
  shm_channel* shm_ch_;

  std::unique_ptr<RingBufferView<T_DATA>> data_buffer_view_;
  std::unique_ptr<RingBufferView<T_DESC>> desc_buffer_view_;
};
