/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 *
 */

#pragma once

#include "cri_registers.hpp"
#include "data_structures.hpp"
#include "dma_channel.hpp"
#include "pda/device.hpp"
#include "pda/pci_bar.hpp"
#include "register_file_bar.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <memory>

namespace cri {

class dma_channel;

// TODO: fix clocks
constexpr uint32_t pkt_clk = 250E6;
constexpr uint32_t gtx_clk = 161132813; // 6,4e-9*64/66

class cri_link {

public:
  cri_link(size_t link_index, pda::device* dev, pda::pci_bar* bar);
  ~cri_link();

  void init_dma(void* data_buffer,
                size_t data_buffer_log_size,
                void* desc_buffer,
                size_t desc_buffer_log_size);

  void deinit_dma();

  void set_testreg_dma(uint32_t data);
  uint32_t get_testreg_dma();
  void set_testreg_data(uint32_t data);
  uint32_t get_testreg_data();

  typedef enum {
    rx_disable = 0x0,
    rx_user = 0x1,
    rx_pgen = 0x2
  } data_sel_t;

  void set_data_sel(data_sel_t rx_sel);
  data_sel_t data_sel();

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
  //  uint32_t get_perf_interval_cycles_gtx();
  uint32_t get_dma_stall();
  uint32_t get_data_buf_stall();
  uint32_t get_desc_buf_stall();
  uint32_t get_event_cnt();
  float get_event_rate();
  uint32_t get_din_full_gtx();
  std::string print_perf_raw();

  /*** Getter ***/
  size_t link_index() { return m_link_index; };
  sys_bus_addr base_addr() { return m_base_addr; };
  pda::device* parent_device() { return m_parent_device; };
  pda::pci_bar* bar() { return m_bar; };

  dma_channel* channel() const;
  register_file* register_file_packetizer() const { return m_rfpkt.get(); }
  register_file* register_file_gtx() const { return m_rfgtx.get(); }

protected:
  std::unique_ptr<dma_channel> m_dma_channel;
  std::unique_ptr<register_file> m_rfpkt;
  std::unique_ptr<register_file> m_rfgtx;

  size_t m_link_index = 0;

  uint32_t m_reg_perf_interval_cached;
  uint32_t m_reg_gtx_perf_interval_cached;

  sys_bus_addr m_base_addr;
  pda::device* m_parent_device;
  pda::pci_bar* m_bar;

  friend std::ostream& operator<<(std::ostream& os,
                                  cri::cri_link::data_sel_t sel);
};
} // namespace cri
