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

constexpr std::array<uint16_t, 1> hw_ver_table_flesin = {{25}};

class flib_device_flesin : public flib_device {

public:
  flib_device_flesin(int device_nr);
  flib_device_flesin(uint8_t bus, uint8_t device, uint8_t function);

  std::vector<flib_link_flesin*> links();
  flib_link_flesin* link(size_t n);

  void id_led(bool enable);

private:
  void init();
};

} /** namespace flib */
