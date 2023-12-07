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

constexpr uint32_t pkt_clk = 250E6;
constexpr uint32_t gtx_clk = 160E6;

class cri_channel {

public:
  cri_channel(size_t ch_index, pda::device* dev, pda::pci_bar* bar);
  ~cri_channel();

  void init_dma(void* data_buffer,
                size_t data_buffer_log_size,
                void* desc_buffer,
                size_t desc_buffer_log_size);

  void deinit_dma();

  void enable_readout();
  void disable_readout();

  void set_testreg_dma(uint32_t data);
  uint32_t get_testreg_dma();
  void set_testreg_data(uint32_t data);
  uint32_t get_testreg_data();

  using data_source_t = enum { rx_disable = 0x0, rx_user = 0x1, rx_pgen = 0x2 };

  void set_data_source(data_source_t src);
  data_source_t data_source();

  void set_ready_for_data(bool enable);
  bool get_ready_for_data();

  void set_mc_size_limit(uint32_t bytes);

  void set_pgen_id(uint16_t eq_id);
  void set_pgen_rate(float val);
  void reset_pgen_mc_pending();
  uint32_t get_pgen_mc_pending();

  using ch_perf_t = struct {
    uint64_t cycles;
    uint64_t dma_trans;
    uint64_t dma_stall;
    uint64_t dma_busy;
    uint64_t data_buf_stall;
    uint64_t desc_buf_stall;
    uint64_t microslice_cnt;
  };

  ch_perf_t get_perf();

  void set_perf_cnt(bool capture, bool reset);
  uint32_t get_perf_cycles();
  uint32_t get_dma_trans();
  uint32_t get_dma_stall();
  uint32_t get_dma_busy();
  uint32_t get_data_buf_stall();
  uint32_t get_desc_buf_stall();
  uint32_t get_microslice_cnt();
  float get_microslice_rate();

  using ch_perf_gtx_t = struct {
    uint64_t cycles;
    uint64_t mc_trans;
    uint64_t mc_stall;
    uint64_t mc_busy;
  };

  ch_perf_gtx_t get_perf_gtx();

  void set_perf_gtx_cnt(bool capture, bool reset);
  uint32_t get_perf_gtx_cycles();
  uint32_t get_mc_trans();
  uint32_t get_mc_stall();
  uint32_t get_mc_busy();

  /*** Getter ***/
  size_t channel_index() const { return m_ch_index; };
  sys_bus_addr base_addr() const { return m_base_addr; };
  pda::device* parent_device() { return m_parent_device; };
  pda::pci_bar* bar() { return m_bar; };

  dma_channel* dma() const;
  register_file* register_file_packetizer() const { return m_rfpkt.get(); }
  register_file* register_file_gtx() const { return m_rfgtx.get(); }

protected:
  std::unique_ptr<dma_channel> m_dma_channel;
  std::unique_ptr<register_file> m_rfpkt;
  std::unique_ptr<register_file> m_rfgtx;

  size_t m_ch_index = 0;

  sys_bus_addr m_base_addr;
  pda::device* m_parent_device;
  pda::pci_bar* m_bar;

  friend std::ostream& operator<<(std::ostream& os,
                                  cri::cri_channel::data_source_t src);
};
} // namespace cri
