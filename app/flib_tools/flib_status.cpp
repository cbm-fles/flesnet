/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib.h"
#include "device_operator.hpp"
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

int main() {

  try {
    std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
    std::vector<std::unique_ptr<flib::flib_device_flesin>> flibs;
    uint64_t num_dev = dev_op->device_count();

    for (size_t i = 0; i < num_dev; ++i) {
      flibs.push_back(std::unique_ptr<flib::flib_device_flesin>(
          new flib::flib_device_flesin(i)));
    }

    size_t j = 0;
    for (auto& flib : flibs) {
      std::cout << "Flib " << j << ": " << flib->print_devinfo() << std::endl;
      ++j;
    }

    std::cout << "\nlink  data_sel  channel_up  hard_err  soft_err  "
                 "eoe_ovfl  d_ovfl  d_max_words\n";
    j = 0;
    for (auto& flib : flibs) {
      size_t num_links = flib->number_of_hw_links();
      std::vector<flib::flib_link_flesin*> links = flib->links();

      if (j != 0) {
        std::cout << "\n";
      }
      std::stringstream ss;
      for (size_t i = 0; i < num_links; ++i) {
        ss << std::setw(2) << j << "/" << i << "  " << std::setw(8)
           << links.at(i)->data_sel() << "  " << std::setw(10)
           << links.at(i)->link_status().channel_up << "  " << std::setw(8)
           << links.at(i)->link_status().hard_err << "  " << std::setw(8)
           << links.at(i)->link_status().soft_err << "  " << std::setw(8)
           << links.at(i)->link_status().eoe_fifo_overflow << "  "
           << std::setw(6) << links.at(i)->link_status().d_fifo_overflow << "  "
           << std::setw(11) << links.at(i)->link_status().d_fifo_max_words
           << "\n";
      }
      std::cout << ss.str();
      ++j;
    }
  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
