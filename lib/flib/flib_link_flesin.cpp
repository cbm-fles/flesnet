/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib_link_flesin.hpp"
#include <arpa/inet.h> // ntohl
#include <cassert>
#include <memory>

namespace flib {

flib_link_flesin::flib_link_flesin(size_t link_index,
                                   pda::device* dev,
                                   pda::pci_bar* bar)
    : flib_link(link_index, dev, bar) {
  m_reg_perf_interval_cached = m_rfpkt->get_reg(RORC_REG_PERF_INTERVAL);
  m_reg_gtx_perf_interval_cached = m_rfgtx->get_reg(RORC_REG_GTX_PERF_INTERVAL);
}

//////*** Readout ***//////

void flib_link_flesin::enable_readout() {}
void flib_link_flesin::disable_readout() {}

flib_link_flesin::link_status_t flib_link_flesin::link_status() {
  uint32_t sts = m_rfgtx->get_reg(RORC_REG_GTX_LINK_STS);

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
void flib_link_flesin::set_perf_interval(uint32_t interval) {
  if (interval > 17000) {
    interval = 17000;
  }
  // pkt domain
  m_reg_perf_interval_cached = interval * (pkt_clk * 1E-3);
  m_rfpkt->set_reg(RORC_REG_PERF_INTERVAL, m_reg_perf_interval_cached);
  // gtx domain
  m_reg_gtx_perf_interval_cached = interval * (gtx_clk * 1E-3);
  m_rfgtx->set_reg(RORC_REG_GTX_PERF_INTERVAL, m_reg_gtx_perf_interval_cached);
}

uint32_t flib_link_flesin::get_perf_interval_cycles_pkt() {
  return m_reg_perf_interval_cached;
}

uint32_t flib_link_flesin::get_perf_interval_cycles_gtx() {
  return m_reg_gtx_perf_interval_cached;
}

// packetizer could not send data (pkt cycles)
uint32_t flib_link_flesin::get_dma_stall() {
  return m_rfpkt->get_reg(RORC_REG_PERF_DMA_STALL);
}

// packatizer stall from data buffer pointer match (pkt cycles)
uint32_t flib_link_flesin::get_data_buf_stall() {
  return m_rfpkt->get_reg(RORC_REG_PERF_EBUF_STALL);
}

// packatizer stall from descriptor buffer pointer match (pkt cycles)
uint32_t flib_link_flesin::get_desc_buf_stall() {
  return m_rfpkt->get_reg(RORC_REG_PERF_RBUF_STALL);
}

// number of events (ref. pkt)
uint32_t flib_link_flesin::get_event_cnt() {
  return m_rfpkt->get_reg(RORC_REG_PERF_N_EVENTS);
}

// event rate in Hz
float flib_link_flesin::get_event_rate() {
  float n_events = static_cast<float>(m_rfpkt->get_reg(RORC_REG_PERF_N_EVENTS));
  return n_events / (static_cast<float>(m_reg_perf_interval_cached) / pkt_clk);
}

// backpressure from packetizer input fifo (gtx cycles)
uint32_t flib_link_flesin::get_din_full_gtx() {
  return m_rfgtx->get_reg(RORC_REG_GTX_PERF_PKT_AFULL);
}

flib_link_flesin::link_perf_t flib_link_flesin::link_perf() {
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

std::string flib_link_flesin::print_perf_raw() {
  std::stringstream ss;
  ss << "pkt_interval " << m_rfpkt->get_reg(RORC_REG_PERF_INTERVAL) << "\n"
     << "gtx_interval " << m_rfgtx->get_reg(RORC_REG_GTX_PERF_INTERVAL) << "\n"
     << "event rate " << m_rfpkt->get_reg(RORC_REG_PERF_N_EVENTS) << "\n"
     << "dma stall " << m_rfpkt->get_reg(RORC_REG_PERF_DMA_STALL) << "\n"
     << "data buf stall " << m_rfpkt->get_reg(RORC_REG_PERF_EBUF_STALL) << "\n"
     << "desc buf stall " << m_rfpkt->get_reg(RORC_REG_PERF_RBUF_STALL) << "\n"
     << "din full " << m_rfgtx->get_reg(RORC_REG_GTX_PERF_PKT_AFULL) << "\n";
  return ss.str();
}

} // namespace flib
