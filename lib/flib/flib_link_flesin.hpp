/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "boost/date_time/posix_time/posix_time.hpp"

#include <flib_link.hpp>

namespace flib {

struct flim_build_info_t {
  boost::posix_time::ptime date;
  uint32_t rev[5];
  std::string host;
  std::string user;
  uint16_t hw_ver;
  bool clean;
  uint8_t repo;
};

typedef enum { link = 0x0, pgen = 0x1 } flim_data_source_t;

struct flim_sts_t {
  bool hard_err;
  bool soft_err;
};

class flib_link_flesin : public flib_link {

public:
  flib_link_flesin(size_t link_index, pda::device* dev, pda::pci_bar* bar);

  void enable_readout(bool enable) override;
  void set_pgen_rate(float val);

  void set_flim_ready_for_data(bool enable);
  void set_film_data_source(flim_data_source_t sel);
  void set_flim_start_idx(uint64_t idx);
  uint64_t get_flim_mc_idx();
  bool get_flim_pgen_present();
  flim_sts_t get_flim_sts();
  void set_flim_pgen_mc_size(uint32_t size);
  void set_flim_pgen_ids(uint16_t eq_id);
  void set_flim_pgen_rate(float val);
  void set_flim_pgen_enable(bool enable);
  void reset_flim_pgen_mc_pending();
  uint32_t get_flim_pgen_mc_pending();

  void set_flim_testreg(uint32_t data);
  uint32_t get_flim_testreg();
  void set_flim_debug_out(bool enable);

  uint16_t flim_hardware_ver();
  uint16_t flim_hardware_id();
  boost::posix_time::ptime flim_build_date();
  std::string flim_build_host();
  std::string flim_build_user();
  struct flim_build_info_t flim_build_info();
  std::string print_flim_build_info();

private:
  std::unique_ptr<register_file> m_rfflim;
};
} // namespace
