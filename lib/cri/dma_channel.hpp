/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 */

#pragma once

#include "cri_channel.hpp"
#include "fles_ipc/MicrosliceDescriptor.hpp"
#include "pda/data_structures.hpp"
#include "pda/dma_buffer.hpp"
#include "register_file.hpp"
#include <memory>
#include <unistd.h> //sysconf

#define BIT_SGENTRY_CTRL_WRITE_EN 31
#define BIT_SGENTRY_CTRL_TARGET 30

#define BIT_DMACTRL_DMA_EN 0
#define BIT_DMACTRL_DATAPATH_RST 1
#define BIT_DMACTRL_EBDM_EN 2
#define BIT_DMACTRL_RBDM_EN 3
#define BIT_DMACTRL_BUSY 7
#define BIT_DMACTRL_TRANS_SIZE_LSB 16
#define BIT_DMACTRL_SYNC_SWRDPTRS 31

namespace cri {

class cri_channel;

class basic_dma_channel {
public:
  basic_dma_channel() = default;
  virtual ~basic_dma_channel() = default;
  virtual void set_sw_read_pointers(uint64_t data_offset,
                                    uint64_t desc_offset) = 0;
  virtual uint64_t get_desc_index() = 0;
  virtual size_t dma_transfer_size() const = 0;
};

class dma_channel : public basic_dma_channel {

public:
  dma_channel(cri_channel* channel,
              void* data_buffer,
              size_t data_buffer_size,
              void* desc_buffer,
              size_t desc_buffer_size,
              size_t dma_transfer_size);

  ~dma_channel();

  void set_sw_read_pointers(uint64_t data_offset,
                            uint64_t desc_offset) override;

  uint64_t get_data_offset();
  uint64_t get_desc_index() override;

  std::string data_buffer_info();
  std::string desc_buffer_info();

  void* data_buffer() const {
    return reinterpret_cast<void*>(m_data_buffer->mem());
  }

  void* desc_buffer() const {
    return reinterpret_cast<void*>(m_desc_buffer->mem());
  }

  void reset_datapath(bool enable);

  size_t dma_transfer_size() const override { return m_dma_transfer_size; }

private:
  enum sg_bram_t { data_sg_bram = 0, desc_sg_bram = 1 };

  using sg_entry_hw_t = struct __attribute__((__packed__)) {
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t length;
  };

  using sw_read_pointers_t = struct __attribute__((__packed__)) {
    uint32_t data_low;
    uint32_t data_high;
    uint32_t desc_low;
    uint32_t desc_high;
    uint32_t dma_ctrl;
  };

  void configure();

  void configure_sg_manager(sg_bram_t buf_sel);

  static std::vector<sg_entry_hw_t>
  convert_sg_list(const std::vector<pda::sg_entry>& sg_list);

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

  inline bool is_enabled() const;

  inline bool is_busy();

  void set_dmactrl(uint32_t reg, uint32_t mask);

  static inline uint32_t
  set_bits(uint32_t old_val, uint32_t new_val, uint32_t mask);

  static inline uint32_t get_lo_32(uint64_t val);

  static inline uint32_t get_hi_32(uint64_t val);

  cri_channel* m_parent_channel;
  register_file* m_rfpkt;
  std::unique_ptr<pda::dma_buffer> m_data_buffer;
  std::unique_ptr<pda::dma_buffer> m_desc_buffer;
  size_t m_dma_transfer_size;
  uint32_t m_reg_dmactrl_cached;
};

} // namespace cri
