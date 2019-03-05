/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#pragma once

#include "pda/device.hpp"
#include "pda/pci_bar.hpp"
#include <array>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace flib {
class flib_device;
class flib_link;

class register_file_bar;

struct build_info_t {
  std::time_t date;
  uint32_t rev[5];
  std::string host;
  std::string user;
  uint16_t hw_ver;
  bool clean;
  uint8_t repo;
};

class flib_device {

public:
  flib_device(int device_nr);
  flib_device(uint8_t bus, uint8_t device, uint8_t function);
  virtual ~flib_device() = 0;

  bool check_hw_ver(std::array<uint16_t, 1> hw_ver_table);
  void enable_mc_cnt(bool enable);
  void set_mc_time(uint32_t time);
  uint8_t number_of_hw_links();

  uint16_t hardware_version();
  time_t build_date();
  std::string build_host();
  std::string build_user();
  struct build_info_t build_info();
  std::string print_build_info();
  std::string print_devinfo();

  size_t number_of_links();
  std::vector<flib_link*> links();
  flib_link* link(size_t n);
  register_file_bar* rf() const;

protected:
  /** Member variables */
  std::unique_ptr<pda::device_operator> m_device_op;
  std::unique_ptr<pda::device> m_device;
  std::unique_ptr<pda::pci_bar> m_bar;
  std::unique_ptr<register_file_bar> m_register_file;
  std::vector<std::unique_ptr<flib_link>> m_link;

  void init();
  bool check_magic_number();
};

} // namespace flib
