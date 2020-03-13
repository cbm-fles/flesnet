/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 */

#pragma once

#include "fles_ipc/MicrosliceDescriptor.hpp"
#include "flib_link.hpp"
#include "pda/data_structures.hpp"
#include "pda/dma_buffer.hpp"
#include "register_file.hpp"
#include <memory>
#include <unistd.h> //sysconf

#define BIT_SGENTRY_CTRL_WRITE_EN 31
#define BIT_SGENTRY_CTRL_TARGET 30

#define BIT_DMACTRL_DMA_EN 0
#define BIT_DMACTRL_FIFO_RST 1
#define BIT_DMACTRL_EBDM_EN 2
#define BIT_DMACTRL_RBDM_EN 3
#define BIT_DMACTRL_BUSY 7
#define BIT_DMACTRL_TRANS_SIZE_LSB 16
#define BIT_DMACTRL_SYNC_SWRDPTRS 31

namespace flib {

class flib_link;

class dma_channel {

public:
  dma_channel(flib_link* link,
              void* data_buffer,
              size_t data_buffer_log_size,
              void* desc_buffer,
              size_t desc_buffer_log_size,
              size_t dma_transfer_size);

  dma_channel(flib_link* link,
              size_t data_buffer_log_size,
              size_t desc_buffer_log_size,
              size_t dma_transfer_size);

  ~dma_channel();

  void set_sw_read_pointers(uint64_t data_offset, uint64_t desc_offset);

  uint64_t get_data_offset();
  uint64_t get_desc_index();

  typedef struct {
    uint64_t nr;
    volatile uint64_t* addr;
    uint32_t size; // bytes
    volatile uint64_t* rbaddr;
  } mc_desc_t;

  std::pair<mc_desc_t, bool> mc();

  int ack_mc();

  std::string data_buffer_info();
  std::string desc_buffer_info();

  void* data_buffer() const {
    return reinterpret_cast<void*>(m_data_buffer->mem());
  }

  void* desc_buffer() const {
    return reinterpret_cast<void*>(m_desc_buffer->mem());
  }

  void reset_fifo(bool enable);

  size_t dma_transfer_size() { return m_dma_transfer_size; }

private:
  enum sg_bram_t { data_sg_bram = 0, desc_sg_bram = 1 };

  typedef struct __attribute__((__packed__)) {
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t length;
  } sg_entry_hw_t;

  typedef struct __attribute__((__packed__)) {
    uint32_t data_low;
    uint32_t data_high;
    uint32_t desc_low;
    uint32_t desc_high;
    uint32_t dma_ctrl;
  } sw_read_pointers_t;

  void configure();

  void configure_sg_manager(sg_bram_t buf_sel);

  static std::vector<sg_entry_hw_t>
  convert_sg_list(const std::vector<pda::sg_entry_t>& sg_list);

  void write_sg_list_to_device(const std::vector<sg_entry_hw_t>& sg_list,
                               sg_bram_t buf_sel);

  void write_sg_entry_to_device(sg_entry_hw_t entry,
                                sg_bram_t buf_sel,
                                uint32_t buf_addr);

  size_t get_max_sg_entries(sg_bram_t buf_sel);

  size_t get_configured_sg_entries(sg_bram_t buf_sel);

  void set_configured_sg_entries(sg_bram_t buf_sel, uint16_t num_entries);

  void set_configured_buffer_size(sg_bram_t buf_sel);

  void set_dma_transfer_size();

  inline void enable();

  void disable(size_t timeout = 10000);

  inline bool is_enabled();

  inline bool is_busy();

  void set_dmactrl(uint32_t reg, uint32_t mask);

  static inline uint32_t
  set_bits(uint32_t old_val, uint32_t new_val, uint32_t mask);

  static inline uint32_t get_lo_32(uint64_t val);

  static inline uint32_t get_hi_32(uint64_t val);

  flib_link* m_parent_link;
  register_file* m_rfpkt;
  std::unique_ptr<pda::dma_buffer> m_data_buffer;
  std::unique_ptr<pda::dma_buffer> m_desc_buffer;
  size_t m_data_buffer_log_size;
  size_t m_desc_buffer_log_size;
  size_t m_dma_transfer_size;
  uint32_t m_reg_dmactrl_cached;

  volatile uint64_t* m_eb = nullptr;
  volatile struct fles::MicrosliceDescriptor* m_db = nullptr;
  uint64_t m_index = 0;
  uint64_t m_last_index = 0;
  uint64_t m_last_acked = 0;
  uint64_t m_mc_nr = 0;
  uint64_t m_wrap = 0;
  uint64_t m_dbentries = 0;
};

} // namespace flib
