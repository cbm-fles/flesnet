/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 *
 */

#include "cri_channel.hpp"
#include <arpa/inet.h> // ntohl
#include <cassert>
#include <memory>

#define DMA_TRANSFER_SIZE 128

namespace cri {
cri_channel::cri_channel(size_t ch_index, pda::device* dev, pda::pci_bar* bar)
    : m_ch_index(ch_index), m_parent_device(dev), m_bar(bar) {
  m_base_addr = (m_ch_index + 1) * (1 << CRI_C_CH_ADDR_SEL);
  // register file access
  m_rfpkt =
      std::unique_ptr<register_file>(new register_file_bar(bar, m_base_addr));
  m_rfgtx = std::unique_ptr<register_file>(
      new register_file_bar(bar, (m_base_addr + (1 << CRI_C_DMA_ADDR_SEL))));
}

cri_channel::~cri_channel() { deinit_dma(); }

void cri_channel::init_dma(void* data_buffer,
                           size_t data_buffer_size,
                           void* desc_buffer,
                           size_t desc_buffer_size) {

  m_dma_channel = std::make_unique<dma_channel>(
      this, data_buffer, data_buffer_size, desc_buffer, desc_buffer_size,
      DMA_TRANSFER_SIZE);
}

void cri_channel::deinit_dma() { m_dma_channel = nullptr; }

// TODO, these may set ready for data and similar
void cri_channel::enable_readout() { set_ready_for_data(true); }

void cri_channel::disable_readout() { set_ready_for_data(false); }

dma_channel* cri_channel::dma() const {
  if (m_dma_channel) {
    return m_dma_channel.get();
  }
  throw CriException("DMA channel not initialized");
}

void cri_channel::set_testreg_dma(uint32_t data) {
  m_rfpkt->set_reg(CRI_REG_TESTREG_DMA, data);
}

uint32_t cri_channel::get_testreg_dma() {
  return m_rfpkt->get_reg(CRI_REG_TESTREG_DMA);
}

void cri_channel::set_testreg_data(uint32_t data) {
  m_rfgtx->set_reg(CRI_REG_TESTREG_DATA, data);
}

uint32_t cri_channel::get_testreg_data() {
  return m_rfgtx->get_reg(CRI_REG_TESTREG_DATA);
}

void cri_channel::set_data_source(data_source_t src) {
  m_rfgtx->set_reg(CRI_REG_GTX_DATAPATH_CFG, src, 0x3);
}

cri_channel::data_source_t cri_channel::data_source() {
  uint32_t dp_cfg = m_rfgtx->get_reg(CRI_REG_GTX_DATAPATH_CFG);
  return static_cast<data_source_t>(dp_cfg & 0x3);
}

std::ostream& operator<<(std::ostream& os,
                         cri::cri_channel::data_source_t src) {
  switch (src) {
  case cri::cri_channel::rx_disable:
    os << "disable";
    break;
  case cri::cri_channel::rx_user:
    os << "   flim";
    break;
  case cri::cri_channel::rx_pgen:
    os << "   pgen";
    break;
  default:
    // os.setstate(std::ios_base::failbit); will stall output
    os << "invalid";
  }
  return os;
}

void cri_channel::set_ready_for_data(bool enable) {
  m_rfgtx->set_bit(CRI_REG_GTX_DATAPATH_CFG, 2, enable);
}

bool cri_channel::get_ready_for_data() {
  return m_rfgtx->get_bit(CRI_REG_GTX_DATAPATH_CFG, 2);
}

void cri_channel::set_mc_size_limit(uint32_t bytes) {
  uint32_t words = bytes / 8; // sizeof(word) == 64Bit
  m_rfpkt->set_reg(CRI_REG_MAX_MC_WORDS, words);
}

//////*** Pattern Generator Configuration ***//////

// TODO: there could be an additional single call for MC_PGEN_CFG_L
void cri_channel::set_pgen_id(uint16_t eq_id) {
  m_rfgtx->set_reg(CRI_REG_GTX_MC_PGEN_CFG_L, eq_id, 0xFFFF);
}

void cri_channel::set_pgen_rate(float val) {
  assert(val >= 0);
  assert(val <= 1);
  uint16_t reg_val =
      static_cast<uint16_t>(static_cast<float>(UINT16_MAX) * (1.0 - val));
  m_rfgtx->set_reg(CRI_REG_GTX_MC_PGEN_CFG_L,
                   static_cast<uint32_t>(reg_val) << 16, 0xFFFF0000);
}

void cri_channel::reset_pgen_mc_pending() {
  m_rfgtx->set_bit(CRI_REG_GTX_MC_PGEN_CFG_H, 0, true); // pulse bit
}

uint32_t cri_channel::get_pgen_mc_pending() {
  return m_rfgtx->get_reg(CRI_REG_GTX_MC_PGEN_MC_PENDING);
}

//////*** Performance Counters ***//////

// synchonously capture and/or reset all perf counters
void cri_channel::set_perf_cnt(bool capture, bool reset) {
  uint32_t reg = 0;
  reg |= ((static_cast<int>(capture) << 1) | static_cast<int>(reset));
  m_rfpkt->set_reg(CRI_REG_PKT_PERF_CFG, reg, 0x3);
}

// get perf interval in clock cycles
uint32_t cri_channel::get_perf_cycles() {
  return m_rfpkt->get_reg(CRI_REG_PKT_PERF_CYCLE);
}

// words accepted by the dma fifo (cycles)
uint32_t cri_channel::get_dma_trans() {
  return m_rfpkt->get_reg(CRI_REG_PKT_PERF_DMA_TRANS);
}

// word not accepted due to back pressure from dma fifo (cycles)
uint32_t cri_channel::get_dma_stall() {
  return m_rfpkt->get_reg(CRI_REG_PKT_PERF_DMA_STALL);
}

// back pressure from dma fifo which DMA idle (cycles)
uint32_t cri_channel::get_dma_busy() {
  return m_rfpkt->get_reg(CRI_REG_PKT_PERF_DMA_BUSY);
}

// packatizer stall from data buffer pointer match (cycles)
uint32_t cri_channel::get_data_buf_stall() {
  return m_rfpkt->get_reg(CRI_REG_PKT_PERF_EBUF_STALL);
}

// packatizer stall from descriptor buffer pointer match (cycles)
uint32_t cri_channel::get_desc_buf_stall() {
  return m_rfpkt->get_reg(CRI_REG_PKT_PERF_RBUF_STALL);
}

// number of microslices (ref. pkt)
uint32_t cri_channel::get_microslice_cnt() {
  return m_rfpkt->get_reg(CRI_REG_PKT_PERF_N_EVENTS);
}

cri_channel::ch_perf_t cri_channel::get_perf() {
  ch_perf_t perf;
  // capture and rest perf counters
  set_perf_cnt(true, true);
  // return shadowed counters
  perf.cycles = get_perf_cycles();
  perf.dma_trans = get_dma_trans();
  perf.dma_stall = get_dma_stall();
  perf.dma_busy = get_dma_busy();
  perf.data_buf_stall = get_data_buf_stall();
  perf.desc_buf_stall = get_desc_buf_stall();
  perf.microslice_cnt = get_microslice_cnt();
  return perf;
}

//////*** Performance Counters Data/GTX Domain ***//////

// synchonously capture and/or reset all perf counters
void cri_channel::set_perf_gtx_cnt(bool capture, bool reset) {
  uint32_t reg = 0;
  reg |= ((static_cast<int>(capture) << 1) | static_cast<int>(reset));
  m_rfgtx->set_reg(CRI_REG_GTX_PERF_CFG, reg, 0x3);
}

// get perf interval in clock cycles
uint32_t cri_channel::get_perf_gtx_cycles() {
  return m_rfgtx->get_reg(CRI_REG_GTX_PERF_CYCLE);
}

// words accepted by the mc interface (cycles)
uint32_t cri_channel::get_mc_trans() {
  return m_rfgtx->get_reg(CRI_REG_GTX_PERF_MC_TRANS);
}

// word not accepted due to back pressure from mc interface (cycles)
uint32_t cri_channel::get_mc_stall() {
  return m_rfgtx->get_reg(CRI_REG_GTX_PERF_MC_STALL);
}

// back pressure from mc interface which input idle (cycles)
uint32_t cri_channel::get_mc_busy() {
  return m_rfgtx->get_reg(CRI_REG_GTX_PERF_MC_BUSY);
}

cri_channel::ch_perf_gtx_t cri_channel::get_perf_gtx() {
  ch_perf_gtx_t perf;
  // capture and rest perf counters
  set_perf_gtx_cnt(true, true);
  // return shadowed counters
  perf.cycles = get_perf_gtx_cycles();
  perf.mc_trans = get_mc_trans();
  perf.mc_stall = get_mc_stall();
  perf.mc_busy = get_mc_busy();
  return perf;
}

} // namespace cri
