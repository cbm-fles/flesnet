/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 */

#include "dma_channel.hpp"
#include "cri_registers.hpp"
#include "data_structures.hpp"
#include <cassert>
#include <cstring>

namespace cri {

// constructor for using user buffers
dma_channel::dma_channel(cri_channel* parent_channel,
                         void* data_buffer,
                         size_t data_buffer_log_size,
                         void* desc_buffer,
                         size_t desc_buffer_log_size,
                         size_t dma_transfer_size)
    : m_parent_channel(parent_channel), m_dma_transfer_size(dma_transfer_size) {
  m_rfpkt = m_parent_channel->register_file_packetizer();
  m_reg_dmactrl_cached = m_rfpkt->get_reg(CRI_REG_DMA_CTRL);
  // ensure HW is disabled
  // TODO: add global lock, otherwise this gives a race condition
  if (is_enabled()) {
    throw CriException("DMA Engine already enabled");
  }
  m_data_buffer = std::unique_ptr<pda::dma_buffer>(
      new pda::dma_buffer(m_parent_channel->parent_device(), data_buffer,
                          (UINT64_C(1) << data_buffer_log_size),
                          (2 * m_parent_channel->channel_index() + 0)));

  m_desc_buffer = std::unique_ptr<pda::dma_buffer>(
      new pda::dma_buffer(m_parent_channel->parent_device(), desc_buffer,
                          (UINT64_C(1) << desc_buffer_log_size),
                          (2 * m_parent_channel->channel_index() + 1)));
  // clear eb for debugging
  memset(m_data_buffer->mem(), 0, m_data_buffer->size());
  // clear rb for polling
  memset(m_desc_buffer->mem(), 0, m_desc_buffer->size());

  configure();
  enable();
}

dma_channel::~dma_channel() {
  disable();
}

// The DMA engine uses the following read/write pointer logic:
// - Pointers show the next element to be read or written
//   or in other words the number of read/written elements.
// - If pointers match the buffer is completly empty.
// - Maximum fill of the buffer is size-1.
// - no writes if: wr_ptr + 1 = rd_ptr
// - no reads if:  wr_ptr = rd_ptr
//
// Pointers return number of bytes. However the DMA engine internally
// handles only entries of size dma_transfer_size and MicrosliceDescriptor.
// Therefore the updated pointes have to aligen to these elements.
//
// The DMA engine provides indices in addition to pointers
// - Indices grow monotonously.
// - The descrioptor index gives the number of written descript.
// - The data index gives the number od written bytes in the data buffer.
//
void dma_channel::set_sw_read_pointers(uint64_t data_offset,
                                       uint64_t desc_offset) {
  assert(data_offset % m_dma_transfer_size == 0);
  assert(desc_offset % sizeof(fles::MicrosliceDescriptor) == 0);

  sw_read_pointers_t offsets;
  offsets.data_low = get_lo_32(data_offset);
  offsets.data_high = get_hi_32(data_offset);
  offsets.desc_low = get_lo_32(desc_offset);
  offsets.desc_high = get_hi_32(desc_offset);

  // TODO: hack to save read-modify-write on dma_ctrl register
  // move sync pointers to exclusive HW register to avoid this
  // no need to chache sync pointers bit because it is pulse only
  offsets.dma_ctrl = m_reg_dmactrl_cached | (1 << BIT_DMACTRL_SYNC_SWRDPTRS);

  m_rfpkt->set_mem(CRI_REG_EBDM_SW_READ_POINTER_L, &offsets,
                   sizeof(offsets) >> 2);
}

uint64_t dma_channel::get_data_offset() {
  uint64_t offset = 0;
  m_rfpkt->get_mem(CRI_REG_EBDM_OFFSET_L, &offset, 2);
  return offset;
}

// get descriptor count from HW
uint64_t dma_channel::get_desc_index() {
  uint64_t index = 0;
  m_rfpkt->get_mem(CRI_REG_DESC_CNT_L, &index, 2);
  return index;
}

std::string dma_channel::data_buffer_info() {
  return m_data_buffer->print_buffer_info();
}

std::string dma_channel::desc_buffer_info() {
  return m_desc_buffer->print_buffer_info();
}

// PRIVATE MEMBERS /////////////////////////////////////

void dma_channel::configure() {
  configure_sg_manager(data_sg_bram);
  configure_sg_manager(desc_sg_bram);
  set_dma_transfer_size();
  set_sw_read_pointers(0, 0);
}

void dma_channel::configure_sg_manager(sg_bram_t buf_sel) {
  pda::dma_buffer* buffer =
      buf_sel != 0u ? m_desc_buffer.get() : m_data_buffer.get();
  // convert list
  std::vector<sg_entry_hw_t> sg_list_hw = convert_sg_list(buffer->sg_list());

  // check that sg list fits into HW bram
  if (sg_list_hw.size() > get_max_sg_entries(buf_sel)) {
    throw CriException("Number of SG entries exceeds available HW memory");
  }
  // write sg list to bram
  write_sg_list_to_device(sg_list_hw, buf_sel);
  set_configured_buffer_size(buf_sel);
}

// TODO make vector a reference
std::vector<dma_channel::sg_entry_hw_t>
dma_channel::convert_sg_list(const std::vector<pda::sg_entry_t>& sg_list) {

  // convert pda scatter gather list into CRI usable list
  std::vector<sg_entry_hw_t> sg_list_hw;
  sg_entry_hw_t hw_entry;
  uint64_t cur_addr;
  uint64_t cur_length;

  for (const auto& entry : sg_list) {
    cur_addr = reinterpret_cast<uint64_t>(entry.pointer);
    cur_length = entry.length;

    // split entries larger than 4GB (addr >32Bit)
    while (cur_length >> 32 != 0) {
      hw_entry.addr_low = get_lo_32(cur_addr);
      hw_entry.addr_high = get_hi_32(cur_addr);
      hw_entry.length =
          (UINT64_C(1) << 32) - PAGE_SIZE; // TODO: why -page_size?
      sg_list_hw.push_back(hw_entry);

      cur_addr += hw_entry.length;
      cur_length -= hw_entry.length;
    }
    hw_entry.addr_low = get_lo_32(cur_addr);
    hw_entry.addr_high = get_hi_32(cur_addr);
    hw_entry.length = static_cast<uint32_t>(cur_length);
    sg_list_hw.push_back(hw_entry);
  }

  return sg_list_hw;
}

void dma_channel::write_sg_list_to_device(
    const std::vector<sg_entry_hw_t>& sg_list, sg_bram_t buf_sel) {
  uint32_t buf_addr = 0;
  for (const auto& entry : sg_list) {
    write_sg_entry_to_device(entry, buf_sel, buf_addr);
    ++buf_addr;
  }
  // clear trailing sg entry in bram
  sg_entry_hw_t clear = sg_entry_hw_t();
  write_sg_entry_to_device(clear, buf_sel, buf_addr);

  // set number of configured sg entries
  // this HW register does not influence the dma engine
  // they are for debug purpose only
  set_configured_sg_entries(buf_sel, sg_list.size());
}

void dma_channel::write_sg_entry_to_device(sg_entry_hw_t entry,
                                           sg_bram_t buf_sel,
                                           uint32_t buf_addr) {

  uint32_t sg_ctrl = (1 << BIT_SGENTRY_CTRL_WRITE_EN) | buf_addr;
  sg_ctrl |= (buf_sel << BIT_SGENTRY_CTRL_TARGET);

  // write entry en block to mailbox register
  m_rfpkt->set_mem(CRI_REG_SGENTRY_ADDR_LOW, &entry, sizeof(entry) >> 2);
  // push mailbox to bram
  m_rfpkt->set_reg(CRI_REG_SGENTRY_CTRL, sg_ctrl);
}

size_t dma_channel::get_max_sg_entries(sg_bram_t buf_sel) {

  sys_bus_addr addr =
      (buf_sel) != 0u ? CRI_REG_RBDM_N_SG_CONFIG : CRI_REG_EBDM_N_SG_CONFIG;
  // N_SG_CONFIG:
  // [15:0] : actual number of sg entries in RAM
  // [31:16]: maximum number of entries (read only)
  return (m_rfpkt->get_reg(addr) >> 16);
}

size_t dma_channel::get_configured_sg_entries(sg_bram_t buf_sel) {

  sys_bus_addr addr =
      (buf_sel) != 0u ? CRI_REG_RBDM_N_SG_CONFIG : CRI_REG_EBDM_N_SG_CONFIG;
  // N_SG_CONFIG:
  // [15:0] : actual number of sg entries in RAM
  // [31:16]: maximum number of entries (read only)
  return (m_rfpkt->get_reg(addr) & 0x0000ffff);
}

void dma_channel::set_configured_sg_entries(sg_bram_t buf_sel,
                                            uint16_t num_entries) {

  sys_bus_addr addr =
      (buf_sel) != 0u ? CRI_REG_RBDM_N_SG_CONFIG : CRI_REG_EBDM_N_SG_CONFIG;
  // N_SG_CONFIG:
  // [15:0] : actual number of sg entries in RAM
  // [31:16]: maximum number of entries (read only)
  m_rfpkt->set_reg(addr, num_entries);
}

void dma_channel::set_configured_buffer_size(sg_bram_t buf_sel) {
  // this HW register does not influence the dma engine
  // they are for debug purpose only
  pda::dma_buffer* buffer =
      (buf_sel) != 0u ? m_desc_buffer.get() : m_data_buffer.get();
  sys_bus_addr addr =
      (buf_sel) != 0u ? CRI_REG_RBDM_BUFFER_SIZE_L : CRI_REG_EBDM_BUFFER_SIZE_L;
  uint64_t size = buffer->size();
  m_rfpkt->set_mem(addr, &size, sizeof(size) >> 2);
}

void dma_channel::set_dma_transfer_size() {
  // dma_transfer_size must be a multiple of 4 byte
  assert((m_dma_transfer_size & 0x3) == 0);
  if (m_dma_transfer_size >
      m_parent_channel->parent_device()->max_payload_size()) {
    throw CriException("DMA transfer size exceeds PCI MaxPayload");
  }
  // set DMA_TRANSFER_SIZE size (has to be provided as #DWs)
  set_dmactrl((m_dma_transfer_size >> 2) << BIT_DMACTRL_TRANS_SIZE_LSB,
              0x3ff << BIT_DMACTRL_TRANS_SIZE_LSB);
}

inline void dma_channel::enable() {
  // split into two requests to allow logic to come out of reset
  // for now we trust in sufficient delay between the requests
  set_dmactrl(0 << BIT_DMACTRL_DATAPATH_RST, 1 << BIT_DMACTRL_DATAPATH_RST);
  set_dmactrl((1 << BIT_DMACTRL_EBDM_EN | 1 << BIT_DMACTRL_RBDM_EN |
               1 << BIT_DMACTRL_DMA_EN),
              (1 << BIT_DMACTRL_EBDM_EN | 1 << BIT_DMACTRL_RBDM_EN |
               1 << BIT_DMACTRL_DMA_EN));
}

void dma_channel::disable(size_t timeout) {
  // disable the DMA engine
  set_dmactrl((0 << BIT_DMACTRL_DMA_EN), (1 << BIT_DMACTRL_DMA_EN));
  // wait till ongoing transfer is finished
  while (is_busy() && timeout != 0) {
    usleep(100);
    --timeout;
    // TODO: add timeout feedback
  }
  // disable everything else and put to reset
  set_dmactrl((0 << BIT_DMACTRL_RBDM_EN | 0 << BIT_DMACTRL_EBDM_EN),
              (1 << BIT_DMACTRL_RBDM_EN | 1 << BIT_DMACTRL_EBDM_EN));
  usleep(100);
  set_dmactrl(1 << BIT_DMACTRL_DATAPATH_RST, 1 << BIT_DMACTRL_DATAPATH_RST);
}

void dma_channel::reset_datapath(bool enable) {
  set_dmactrl((static_cast<int>(enable) << BIT_DMACTRL_DATAPATH_RST),
              (1 << BIT_DMACTRL_DATAPATH_RST));
}

inline bool dma_channel::is_enabled() {
  uint32_t mask = 1 << BIT_DMACTRL_EBDM_EN | 1 << BIT_DMACTRL_RBDM_EN |
                  1 << BIT_DMACTRL_DMA_EN;
  return (m_reg_dmactrl_cached & mask) != 0u;
}

inline bool dma_channel::is_busy() {
  return m_rfpkt->get_bit(CRI_REG_DMA_CTRL, BIT_DMACTRL_BUSY);
}

void dma_channel::set_dmactrl(uint32_t reg, uint32_t mask) {
  // acces to dma_ctrl register
  // ensures proper caching of register value
  // never cache BIT_DMACTRL_SYNC_SWRDPTRS
  mask &= ~(1 << BIT_DMACTRL_SYNC_SWRDPTRS);
  m_reg_dmactrl_cached = set_bits(m_reg_dmactrl_cached, reg, mask);
  m_rfpkt->set_reg(CRI_REG_DMA_CTRL, m_reg_dmactrl_cached);
}

inline uint32_t
dma_channel::set_bits(uint32_t old_val, uint32_t new_val, uint32_t mask) {
  // clear all unselected bits in new_val
  new_val &= mask;
  // clear all selects bits and set according to new_val
  old_val &= ~mask;
  old_val |= new_val;
  return old_val;
}

inline uint32_t dma_channel::get_lo_32(uint64_t val) { return val; }

inline uint32_t dma_channel::get_hi_32(uint64_t val) { return (val >> 32); }

} // namespace cri
