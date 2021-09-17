/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "pda/device.hpp"
#include "pda/pci_bar.hpp"
#include <array>
#include <chrono>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace cri {

// class cri_device;
class cri_link;
class register_file_bar;

constexpr std::array<uint16_t, 1> hw_ver_table = {{2}};
constexpr uint32_t pci_clk = 250E6;

constexpr size_t pgen_base_size_ns = 1000;

struct build_info_t {
  std::time_t date;
  uint32_t rev[5];
  std::string host;
  std::string user;
  uint16_t hw_ver;
  bool clean;
  uint8_t repo;
};

class cri_device {

public:
  cri_device(int device_nr);
  cri_device(uint8_t bus, uint8_t device, uint8_t function);
  ~cri_device();

  bool check_hw_ver(std::array<uint16_t, 1> hw_ver_table);
  void enable_mc_cnt(bool enable);
  void set_pgen_mc_size(uint32_t mc_size);
  size_t get_pgen_base_size_ns() { return pgen_base_size_ns; }
  uint8_t number_of_hw_links();

  uint16_t hardware_version();
  time_t build_date();
  std::string build_host();
  std::string build_user();
  struct build_info_t build_info();
  std::string print_build_info();
  std::string print_devinfo();
  std::chrono::seconds uptime();
  std::string print_uptime();

  void set_testreg(uint32_t data);
  uint32_t get_testreg();

  size_t number_of_links();
  std::vector<cri_link*> links();
  cri_link* link(size_t n);
  register_file_bar* rf() const;

  void id_led(bool enable);

  typedef struct {
    uint64_t cycles;
    uint64_t pci_trans;
    uint64_t pci_stall;
    uint64_t pci_busy;
    uint64_t pci_max_stall;
  } dev_perf_t;

  dev_perf_t get_perf();

  void set_perf_cnt(bool capture, bool reset);
  uint32_t get_perf_cycles();
  uint32_t get_pci_trans();
  uint32_t get_pci_stall();
  uint32_t get_pci_busy();
  float get_pci_max_stall();

protected:
  /** Member variables */
  std::unique_ptr<pda::device_operator> m_device_op;
  std::unique_ptr<pda::device> m_device;
  std::unique_ptr<pda::pci_bar> m_bar;
  std::unique_ptr<register_file_bar> m_register_file;
  std::vector<std::unique_ptr<cri_link>> m_link;

  void init();
  bool check_magic_number();
};

} // namespace cri
