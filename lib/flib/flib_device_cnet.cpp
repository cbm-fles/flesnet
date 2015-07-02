/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <flib_device_cnet.hpp>
#include <flib_link_cnet.hpp>

namespace flib {

flib_device_cnet::flib_device_cnet(int device_nr) : flib_device(device_nr) {

  if (!check_hw_ver(hw_ver_table_cnet)) {
    throw FlibException("Hardware - libflib version missmatch!");
  }

  // create link objects
  uint8_t num_links = number_of_hw_links();
  for (size_t i = 0; i < num_links; i++) {
    m_link.push_back(std::unique_ptr<flib_link>(
        new flib_link_cnet(i, m_device.get(), m_bar.get())));
  }
}

std::vector<flib_link_cnet*> flib_device_cnet::links() {
  std::vector<flib_link_cnet*> links;
  for (auto& l : m_link) {
    links.push_back(static_cast<flib_link_cnet*>(l.get()));
  }
  return links;
}

flib_link_cnet* flib_device_cnet::link(size_t n) {
  return static_cast<flib_link_cnet*>(m_link.at(n).get());
}

void flib_device_cnet::send_dlm() {
  m_register_file->set_reg(RORC_REG_DLM_CFG, 1);
}
}
