/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 *
 */

#include "cri_link.hpp"
#include <arpa/inet.h> // ntohl
#include <cassert>
#include <memory>

#define DMA_TRANSFER_SIZE 128

namespace cri {
cri_link::cri_link(size_t link_index, pda::device* dev, pda::pci_bar* bar)
    : m_link_index(link_index), m_parent_device(dev), m_bar(bar) {
  m_base_addr = (m_link_index + 1) * (1 << CRI_C_CH_ADDR_SEL);
  // register file access
  m_rfpkt =
      std::unique_ptr<register_file>(new register_file_bar(bar, m_base_addr));
  m_rfgtx = std::unique_ptr<register_file>(
      new register_file_bar(bar, (m_base_addr + (1 << CRI_C_DMA_ADDR_SEL))));
  // initialize cached members
  m_reg_perf_interval_cached = m_rfpkt->get_reg(CRI_REG_PERF_INTERVAL);
  m_reg_gtx_perf_interval_cached = m_rfgtx->get_reg(CRI_REG_GTX_PERF_INTERVAL);
}

cri_link::~cri_link() { deinit_dma(); }

void cri_link::init_dma(void* data_buffer,
                        size_t data_buffer_log_size,
                        void* desc_buffer,
                        size_t desc_buffer_log_size) {

  m_dma_channel = std::unique_ptr<dma_channel>(
      new dma_channel(this, data_buffer, data_buffer_log_size, desc_buffer,
                      desc_buffer_log_size, DMA_TRANSFER_SIZE));
}

void cri_link::init_dma(size_t data_buffer_log_size,
                        size_t desc_buffer_log_size) {

  m_dma_channel = std::unique_ptr<dma_channel>(new dma_channel(
      this, data_buffer_log_size, desc_buffer_log_size, DMA_TRANSFER_SIZE));
}

void cri_link::deinit_dma() { m_dma_channel = nullptr; }

dma_channel* cri_link::channel() const {
  if (m_dma_channel) {
    return m_dma_channel.get();
  }
  throw CriException("DMA channel not initialized");
}

//////*** DPB Emualtion ***//////

void cri_link::reset_datapath() {
  // disable readout if still enabled
  disable_readout();
  // datapath reset, will also cause hw defaults for
  // - pending mc  = 0
  m_rfgtx->set_bit(CRI_REG_GTX_DATAPATH_CFG, 2, true);
  // TODO this may be needed in case of errors
  // if (m_dma_channel) {
  //  m_dma_channel->reset_fifo(true);
  //}
  m_rfgtx->set_bit(CRI_REG_GTX_DATAPATH_CFG, 2, false);
}

void cri_link::reset_link() {
  m_rfgtx->set_bit(CRI_REG_GTX_DATAPATH_CFG, 3, true);
  usleep(1000);
  m_rfgtx->set_bit(CRI_REG_GTX_DATAPATH_CFG, 3, false);
}

void cri_link::set_data_sel(data_sel_t rx_sel) {
  uint32_t dp_cfg = m_rfgtx->get_reg(CRI_REG_GTX_DATAPATH_CFG);
  switch (rx_sel) {
  case rx_disable:
    m_rfgtx->set_reg(CRI_REG_GTX_DATAPATH_CFG, (dp_cfg & ~3));
    break;
  case rx_link:
    m_rfgtx->set_reg(CRI_REG_GTX_DATAPATH_CFG, ((dp_cfg | (1 << 1)) & ~1));
    break;
  case rx_pgen:
    m_rfgtx->set_reg(CRI_REG_GTX_DATAPATH_CFG, (dp_cfg | 3));
    break;
  case rx_emu:
    m_rfgtx->set_reg(CRI_REG_GTX_DATAPATH_CFG, ((dp_cfg | 1)) & ~(1 << 1));
    break;
  }
}

cri_link::data_sel_t cri_link::data_sel() {
  uint32_t dp_cfg = m_rfgtx->get_reg(CRI_REG_GTX_DATAPATH_CFG);
  return static_cast<data_sel_t>(dp_cfg & 0x3);
}

std::ostream& operator<<(std::ostream& os, cri::cri_link::data_sel_t sel) {
  switch (sel) {
  case cri::cri_link::rx_disable:
    os << "disable";
    break;
  case cri::cri_link::rx_emu:
    os << "    emu";
    break;
  case cri::cri_link::rx_link:
    os << "   link";
    break;
  case cri::cri_link::rx_pgen:
    os << "   pgen";
    break;
  default:
    os.setstate(std::ios_base::failbit);
  }
  return os;
}

//////*** Readout ***//////

void cri_link::enable_readout() {}
void cri_link::disable_readout() {}

cri_link::link_status_t cri_link::link_status() {
  uint32_t sts = m_rfgtx->get_reg(CRI_REG_GTX_LINK_STS);

  link_status_t link_status;
  link_status.channel_up = ((sts & (1 << 29)) != 0u);
  link_status.hard_err = ((sts & (1 << 28)) != 0u);
  link_status.soft_err = ((sts & (1 << 27)) != 0u);
  link_status.eoe_fifo_overflow = ((sts & (1 << 31)) != 0u);
  link_status.d_fifo_overflow = ((sts & (1 << 30)) != 0u);
  link_status.d_fifo_max_words = (sts & 0x7FF);
  return link_status;
}

//////*** Performance Counters ***//////

// set messurement avaraging interval in ms (max 17s)
void cri_link::set_perf_interval(uint32_t interval) {
  if (interval > 17000) {
    interval = 17000;
  }
  // pkt domain
  m_reg_perf_interval_cached = interval * (pkt_clk * 1E-3);
  m_rfpkt->set_reg(CRI_REG_PERF_INTERVAL, m_reg_perf_interval_cached);
  // gtx domain
  m_reg_gtx_perf_interval_cached = interval * (gtx_clk * 1E-3);
  m_rfgtx->set_reg(CRI_REG_GTX_PERF_INTERVAL, m_reg_gtx_perf_interval_cached);
}

uint32_t cri_link::get_perf_interval_cycles_pkt() {
  return m_reg_perf_interval_cached;
}

uint32_t cri_link::get_perf_interval_cycles_gtx() {
  return m_reg_gtx_perf_interval_cached;
}

// packetizer could not send data (pkt cycles)
uint32_t cri_link::get_dma_stall() {
  return m_rfpkt->get_reg(CRI_REG_PERF_DMA_STALL);
}

// packatizer stall from data buffer pointer match (pkt cycles)
uint32_t cri_link::get_data_buf_stall() {
  return m_rfpkt->get_reg(CRI_REG_PERF_EBUF_STALL);
}

// packatizer stall from descriptor buffer pointer match (pkt cycles)
uint32_t cri_link::get_desc_buf_stall() {
  return m_rfpkt->get_reg(CRI_REG_PERF_RBUF_STALL);
}

// number of events (ref. pkt)
uint32_t cri_link::get_event_cnt() {
  return m_rfpkt->get_reg(CRI_REG_PERF_N_EVENTS);
}

// event rate in Hz
float cri_link::get_event_rate() {
  float n_events = static_cast<float>(m_rfpkt->get_reg(CRI_REG_PERF_N_EVENTS));
  return n_events / (static_cast<float>(m_reg_perf_interval_cached) / pkt_clk);
}

// backpressure from packetizer input fifo (gtx cycles)
uint32_t cri_link::get_din_full_gtx() {
  return m_rfgtx->get_reg(CRI_REG_GTX_PERF_PKT_AFULL);
}

cri_link::link_perf_t cri_link::link_perf() {
  link_perf_t perf;
  perf.pkt_cycle_cnt = m_reg_perf_interval_cached;
  perf.dma_stall = get_dma_stall();
  perf.data_buf_stall = get_data_buf_stall();
  perf.desc_buf_stall = get_desc_buf_stall();
  perf.events = get_event_cnt();
  perf.gtx_cycle_cnt = m_reg_gtx_perf_interval_cached;
  perf.din_full_gtx = get_din_full_gtx();
  return perf;
}

std::string cri_link::print_perf_raw() {
  std::stringstream ss;
  ss << "pkt_interval " << m_rfpkt->get_reg(CRI_REG_PERF_INTERVAL) << "\n"
     << "gtx_interval " << m_rfgtx->get_reg(CRI_REG_GTX_PERF_INTERVAL) << "\n"
     << "event rate " << m_rfpkt->get_reg(CRI_REG_PERF_N_EVENTS) << "\n"
     << "dma stall " << m_rfpkt->get_reg(CRI_REG_PERF_DMA_STALL) << "\n"
     << "data buf stall " << m_rfpkt->get_reg(CRI_REG_PERF_EBUF_STALL) << "\n"
     << "desc buf stall " << m_rfpkt->get_reg(CRI_REG_PERF_RBUF_STALL) << "\n"
     << "din full " << m_rfgtx->get_reg(CRI_REG_GTX_PERF_PKT_AFULL) << "\n";
  return ss.str();
}

} // namespace cri
