/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "flib_link_flesin.hpp"
#include <ctime>

namespace flib {

class flim {

public:
  flim(flib_link_flesin* link);

  ~flim();

  typedef enum { user = 0x0, pgen = 0x1 } data_source_t;

  struct sts_t {
    bool hard_err;
    bool soft_err;
  };

  void reset_datapath();
  void set_ready_for_data(bool enable);
  void set_data_source(data_source_t sel);
  uint64_t get_mc_idx();
  uint64_t get_mc_time();
  bool get_pgen_present();
  sts_t get_sts();
  void set_pgen_mc_size(uint32_t size);
  void set_pgen_ids(uint16_t eq_id);
  void set_pgen_rate(float val);
  static void set_pgen_start_time(uint32_t time);
  void set_pgen_enable(bool enable);
  void set_pgen_sync_ext(bool enable);
  void reset_pgen_mc_pending();
  uint32_t get_pgen_mc_pending();
  void set_mc_size_limit(uint32_t bytes);

  void set_testreg(uint32_t data);
  uint32_t get_testreg();
  void set_debug_out(bool enable);

  struct build_info_t {
    time_t date;
    uint32_t rev[5];
    std::string host;
    std::string user;
    uint16_t hw_ver;
    bool clean;
    uint8_t repo;
  };

  uint16_t hardware_ver();
  uint16_t hardware_id();
  time_t build_date();
  std::string build_host();
  std::string build_user();
  struct build_info_t build_info();
  std::string print_build_info();

private:
  std::unique_ptr<register_file> m_rfflim;
};
} // namespace flib
