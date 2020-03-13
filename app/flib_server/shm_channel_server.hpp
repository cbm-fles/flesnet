// Copyright 2015 Dirk Hutter

#pragma once

#include "flib_link.hpp"
#include "log.hpp"
#include "shm_channel.hpp"
#include "shm_device.hpp"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <cstdint>
#include <string>

namespace ip = boost::interprocess;

template <typename T_DESC, typename T_DATA> class shm_channel_server {

public:
  shm_channel_server(ip::managed_shared_memory* shm,
                     shm_device* shm_dev,
                     size_t index,
                     flib::flib_link* flib_link,
                     size_t data_buffer_size_exp,
                     size_t desc_buffer_size_exp)
      : m_shm(shm), m_shm_dev(shm_dev), m_index(index), m_flib_link(flib_link),
        m_data_buffer_size_exp(data_buffer_size_exp),
        m_desc_buffer_size_exp(desc_buffer_size_exp) {

    // allocate buffers
    void* data_buffer_raw = alloc_buffer(data_buffer_size_exp, data_item_size);
    void* desc_buffer_raw = alloc_buffer(desc_buffer_size_exp, desc_item_size);

    // constuct channel exchange object in shared memory
    std::string channel_name = "shm_channel_" + std::to_string(m_index);
    m_shm_ch = m_shm->construct<shm_channel>(channel_name.c_str())(
        m_shm, data_buffer_raw, data_buffer_size_exp, sizeof(T_DATA),
        desc_buffer_raw, desc_buffer_size_exp, sizeof(T_DESC));

    // initialize buffer info
    T_DATA* data_buffer = reinterpret_cast<T_DATA*>(data_buffer_raw);
    T_DESC* desc_buffer = reinterpret_cast<T_DESC*>(desc_buffer_raw);

    m_data_buffer_view = std::unique_ptr<RingBufferView<T_DATA>>(
        new RingBufferView<T_DATA>(data_buffer, data_buffer_size_exp));
    m_desc_buffer_view = std::unique_ptr<RingBufferView<T_DESC>>(
        new RingBufferView<T_DESC>(desc_buffer, desc_buffer_size_exp));

    // initialize flib DMA engine
    static_assert(desc_item_size == (UINT64_C(1) << 5),
                  "incompatible desc_item_size in shm_channel_server");
    static_assert(data_item_size == (UINT64_C(1) << 0),
                  "incompatible data_item_size in shm_channel_server");

    m_flib_link->init_dma(data_buffer_raw, data_buffer_size_exp + 0,
                          desc_buffer_raw, desc_buffer_size_exp + 5);
    m_dma_transfer_size = m_flib_link->channel()->dma_transfer_size();

    m_flib_link->enable_readout();
  }

  ~shm_channel_server() {
    try {
      ip::scoped_lock<ip::interprocess_mutex> lock(m_shm_dev->m_mutex);
      update_write_index(lock);
      m_shm_ch->set_eof(lock, true);
    } catch (ip::interprocess_exception const& e) {
      L_(error) << "Failed to shut down channel: " << e.what();
    }
    m_flib_link->deinit_dma();
    // TODO destroy channel object and deallocate buffers if it is worth to do
  }

  bool check_pending_req(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock); // ensure mutex is really owned
    return m_shm_ch->req_read_index(lock) || m_shm_ch->req_write_index(lock);
  }

  void try_handle_req(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    assert(lock); // ensure mutex is really owned

    if (m_shm_ch->req_read_index(lock)) {
      DualIndex read_index = m_shm_ch->read_index(lock);
      // reset req before releasing lock ensures not to miss last req
      m_shm_ch->set_req_read_index(lock, false);
      lock.unlock();
      L_(trace) << "updating read_index: data " << read_index.data << " desc "
                << read_index.desc;

      m_flib_link->channel()->set_sw_read_pointers(
          hw_pointer(read_index.data, m_data_buffer_size_exp, data_item_size,
                     m_dma_transfer_size),
          hw_pointer(read_index.desc, m_desc_buffer_size_exp, desc_item_size));
      lock.lock();
    }

    if (m_shm_ch->req_write_index(lock)) {
      update_write_index(lock);
    }
  }

private:
  void update_write_index(ip::scoped_lock<ip::interprocess_mutex>& lock) {
    m_shm_ch->set_req_write_index(lock, false);
    lock.unlock();
    // fill write indices
    TimedDualIndex write_index;
    write_index.index.desc = m_flib_link->channel()->get_desc_index();
    write_index.index.data =
        m_desc_buffer_view->at(write_index.index.desc - 1).offset +
        m_desc_buffer_view->at(write_index.index.desc - 1).size;
    write_index.updated = boost::posix_time::microsec_clock::universal_time();
    L_(trace) << "fetching write_index: data " << write_index.index.data
              << " desc " << write_index.index.desc;
    lock.lock();
    m_shm_ch->set_write_index(lock, write_index);
  }

  // Convert index into byte pointer for hardware
  size_t hw_pointer(size_t index, size_t size_exponent, size_t item_size) {
    size_t buffer_size = UINT64_C(1) << size_exponent;
    size_t masked_index = index & (buffer_size - 1);
    return masked_index * item_size;
  }

  // Convert index into byte pointer and round to dma_size
  size_t hw_pointer(size_t index,
                    size_t size_exponent,
                    size_t item_size,
                    size_t dma_size) {
    size_t byte_index = hw_pointer(index, size_exponent, item_size);
    // will hang one transfer size behind
    return byte_index & ~(dma_size - 1);
  }

  void* alloc_buffer(size_t size_exp, size_t item_size) {
    size_t bytes = (UINT64_C(1) << size_exp) * item_size;
    L_(trace) << "allocating shm buffer of " << bytes << " bytes";
    return m_shm->allocate_aligned(bytes, sysconf(_SC_PAGESIZE));
  }

  ip::managed_shared_memory* m_shm;
  shm_device* m_shm_dev;
  size_t m_index;
  flib::flib_link* m_flib_link;
  size_t m_dma_transfer_size;

  shm_channel* m_shm_ch;
  std::unique_ptr<RingBufferView<T_DATA>> m_data_buffer_view;
  std::unique_ptr<RingBufferView<T_DESC>> m_desc_buffer_view;
  size_t m_data_buffer_size_exp;
  size_t m_desc_buffer_size_exp;
  constexpr static size_t data_item_size = sizeof(T_DATA);
  constexpr static size_t desc_item_size = sizeof(T_DESC);
};
