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
    float dma_stall;
    float data_buf_stall;
    float desc_buf_stall;
    float event_rate;
    float din_full;
  } link_perf_t;

  link_perf_t link_perf();

  void set_perf_interval(uint32_t interval);
  float get_dma_stall();
  float get_data_buf_stall();
  float get_desc_buf_stall();
  float get_event_rate();
  float get_din_full();
  std::string print_perf_raw();

private:
  uint32_t m_reg_perf_interval_cached;
  uint32_t m_reg_gtx_perf_interval_cached;
};
} // namespace
