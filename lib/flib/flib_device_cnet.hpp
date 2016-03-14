/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include <registers_cnet.h>
#include <data_structures.hpp>
#include <flib_device.hpp>
#include <flib_link_cnet.hpp>

namespace flib {

constexpr std::array<uint16_t, 1> hw_ver_table_cnet = {{9}};

class flib_device_cnet : public flib_device {

public:
  flib_device_cnet(int device_nr);

  std::vector<flib_link_cnet*> links();
  flib_link_cnet* link(size_t n);

  /** global dlm send, requires link local prepare_dlm beforehand */
  void send_dlm();
};

} /** namespace flib */
