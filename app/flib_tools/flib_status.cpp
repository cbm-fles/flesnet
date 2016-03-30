/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib.h"
#include <iostream>

std::ostream& operator<<(std::ostream& os, flib::flib_link::data_sel_t sel) {
  switch (sel) {
  case flib::flib_link::rx_disable:
    os << "disable";
    break;
  case flib::flib_link::rx_emu:
    os << "    emu";
    break;
  case flib::flib_link::rx_link:
    os << "   link";
    break;
  case flib::flib_link::rx_pgen:
    os << "   pgen";
    break;
  default:
    os.setstate(std::ios_base::failbit);
  }
  return os;
}

std::unique_ptr<flib::flib_device_flesin> Flib;

int main(int argc, char* argv[]) {

  size_t flib_index = 0;
  if (argc == 2) {
    flib_index = atoi(argv[1]);
    std::cout << "Status FLIB " << flib_index << std::endl;
  } else {
    std::cout << "usage: " << argv[0] << " <flib-index>" << std::endl;
    exit(EXIT_FAILURE);
  }

  try {
    Flib = std::unique_ptr<flib::flib_device_flesin>(
        new flib::flib_device_flesin(flib_index));
  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  size_t num_links = Flib->number_of_hw_links();
  std::cout << "Total number of links: " << num_links << std::endl;
  std::vector<flib::flib_link_flesin*> links = Flib->links();

  std::stringstream ss;
  ss << "link  data_sel  channel_up  hard_err  soft_err  "
        "eoe_ovfl  d_ovfl  d_max_words\n";
  for (size_t i = 0; i < num_links; ++i) {
    ss << std::setw(4) << i << "  " << std::setw(8) << links.at(i)->data_sel()
       << "  " << std::setw(10) << links.at(i)->link_status().channel_up << "  "
       << std::setw(8) << links.at(i)->link_status().hard_err << "  "
       << std::setw(8) << links.at(i)->link_status().soft_err << "  "
       << std::setw(8) << links.at(i)->link_status().eoe_fifo_overflow << "  "
       << std::setw(6) << links.at(i)->link_status().d_fifo_overflow << "  "
       << std::setw(11) << links.at(i)->link_status().d_fifo_max_words << "\n";
  }
  std::cout << ss.str();

  return EXIT_SUCCESS;
}
