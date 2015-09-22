// Copyright 2015 Dirk Hutter

#pragma once

#include <cstdint>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "log.hpp"
#include "flib_link.hpp"

#include "shm_channel.hpp"

using namespace boost::interprocess;
using namespace flib;

class shm_channel_server {

public:
  shm_channel_server(managed_shared_memory* shm,
                     size_t index,
                     flib_link* flib_link,
                     size_t data_buffer_size_exp,
                     size_t desc_buffer_size_exp)
      : m_shm(shm), m_index(index), m_flib_link(flib_link),
        m_data_buffer_size_exp(data_buffer_size_exp),
        m_desc_buffer_size_exp(desc_buffer_size_exp) {
    // allocate buffers
    m_data_buffer = alloc_buffer(m_data_buffer_size_exp);
    m_desc_buffer = alloc_buffer(m_desc_buffer_size_exp);

    // constuct channel exchange object in sharde memory
    std::string channel_name =
        "shm_channel_" + boost::lexical_cast<std::string>(m_index);
    m_shm_ch = m_shm->construct<shm_channel>(channel_name.c_str())(
        m_shm, m_data_buffer, m_data_buffer_size_exp, m_desc_buffer,
        m_desc_buffer_size_exp);

    // initialize flib DMA engine
    m_flib_link->init_dma(m_data_buffer, m_data_buffer_size_exp, m_desc_buffer,
                          m_desc_buffer_size_exp);

    m_flib_link->set_start_idx(0);
    m_flib_link->enable_readout(true);
  }

  ~shm_channel_server() {
    m_flib_link->deinit_dma();
    // TODO destroy channel object and deallocate buffers if it is worth to do
  }

  bool check_pending_req(scoped_lock<interprocess_mutex>& lock) {
    assert(lock); // ensure mutex is really owned
    return m_shm_ch->req_read_index(lock) || m_shm_ch->req_write_index(lock);
  }

  void try_handle_req(scoped_lock<interprocess_mutex>& lock) {
    assert(lock); // ensure mutex is really owned

    if (m_shm_ch->req_read_index(lock)) {
      DualIndex read_index = m_shm_ch->read_index(lock);
      // reset req before releasing lock ensures not to miss last req
      m_shm_ch->set_req_read_index(lock, false);
      lock.unlock();
      L_(trace) << "updating read_index: data " << read_index.data << " desc "
                << read_index.desc;

      m_flib_link->channel()->set_sw_read_pointers(read_index.data,
                                                   read_index.desc);
      lock.lock();
    }

    if (m_shm_ch->req_write_index(lock)) {
      m_shm_ch->set_req_write_index(lock, false);
      lock.unlock();
      TimedDualIndex write_index;
      write_index.index.data = m_flib_link->channel()->get_data_offset();
      write_index.index.desc = m_flib_link->mc_index();
      write_index.updated = boost::posix_time::microsec_clock::universal_time();
      L_(trace) << "fetching write_index: data " << write_index.index.data
                << " desc " << write_index.index.desc;

      lock.lock();
      m_shm_ch->set_write_index(lock, write_index);
    }
  }

private:
  void* alloc_buffer(size_t size_exp) {
    L_(trace) << "allocating shm buffer of " << (UINT64_C(1) << size_exp)
              << " bytes";
    return m_shm->allocate_aligned(UINT64_C(1) << size_exp,
                                   sysconf(_SC_PAGESIZE));
  }

  managed_shared_memory* m_shm;
  size_t m_index;
  flib_link* m_flib_link;

  shm_channel* m_shm_ch;
  void* m_data_buffer;
  void* m_desc_buffer;
  size_t m_data_buffer_size_exp;
  size_t m_desc_buffer_size_exp;
};
