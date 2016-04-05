/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib_device_flesin.hpp"
#include "flib_link_flesin.hpp"

namespace flib {

flib_device_flesin::flib_device_flesin(int device_nr) : flib_device(device_nr) {
  init();
}

flib_device_flesin::flib_device_flesin(uint8_t bus,
                                       uint8_t device,
                                       uint8_t function)
    : flib_device(bus, device, function) {
  init();
}

void flib_device_flesin::init() {
  check_hw_ver(hw_ver_table_flesin);
  // create link objects
  uint8_t num_links = number_of_hw_links();
  for (size_t i = 0; i < num_links; i++) {
    m_link.push_back(std::unique_ptr<flib_link>(
        new flib_link_flesin(i, m_device.get(), m_bar.get())));
  }
}

std::vector<flib_link_flesin*> flib_device_flesin::links() {
  std::vector<flib_link_flesin*> links;
  for (auto& l : m_link) {
    links.push_back(static_cast<flib_link_flesin*>(l.get()));
  }
  return links;
}

flib_link_flesin* flib_device_flesin::link(size_t n) {
  return static_cast<flib_link_flesin*>(m_link.at(n).get());
}
}
