/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "data_structures.hpp"
#include "flib_device.hpp"
#include "flib_link_flesin.hpp"

namespace flib {

constexpr std::array<uint16_t, 1> hw_ver_table_flesin = {{26}};
constexpr uint32_t pci_clk = 250E6;

struct dma_perf_data_t {
  uint64_t overflow;
  uint64_t cycle_cnt;
  std::array<uint64_t, 8> fifo_fill;
};

class flib_device_flesin : public flib_device {

public:
  flib_device_flesin(int device_nr);
  flib_device_flesin(uint8_t bus, uint8_t device, uint8_t function);

  std::vector<flib_link_flesin*> links();
  flib_link_flesin* link(size_t n);

  void id_led(bool enable);

  void set_perf_interval(uint32_t interval);
  uint32_t get_perf_interval_cycles();
  uint32_t get_pci_stall();
  uint32_t get_pci_trans();
  float get_pci_max_stall();
  dma_perf_data_t get_dma_perf();

private:
  void init();
  uint32_t m_reg_perf_interval_cached;
};

} /** namespace flib */
