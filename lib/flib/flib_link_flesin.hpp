/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "flib_link.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace flib {

constexpr uint32_t pkt_clk = 250E6;
constexpr uint32_t gtx_clk = 161132813; // 6,4e-9*64/66

class flib_link_flesin : public flib_link {

public:
  flib_link_flesin(size_t link_index, pda::device* dev, pda::pci_bar* bar);

  void enable_readout() override;
  void disable_readout() override;

  typedef struct {
    bool channel_up;
    bool hard_err;
    bool soft_err;
    bool eoe_fifo_overflow;
    bool d_fifo_overflow;
    uint16_t d_fifo_max_words;
  } link_status_t;

  link_status_t link_status();

  typedef struct {
    uint64_t pkt_cycle_cnt;
    uint64_t dma_stall;
    uint64_t data_buf_stall;
    uint64_t desc_buf_stall;
    uint64_t events;
    uint64_t gtx_cycle_cnt;
    uint64_t din_full_gtx;
  } link_perf_t;

  link_perf_t link_perf();

  void set_perf_interval(uint32_t interval);
  uint32_t get_perf_interval_cycles_pkt();
  uint32_t get_perf_interval_cycles_gtx();
  uint32_t get_dma_stall();
  uint32_t get_data_buf_stall();
  uint32_t get_desc_buf_stall();
  uint32_t get_event_cnt();
  float get_event_rate();
  uint32_t get_din_full_gtx();
  std::string print_perf_raw();

private:
  uint32_t m_reg_perf_interval_cached;
  uint32_t m_reg_gtx_perf_interval_cached;
};
} // namespace flib
