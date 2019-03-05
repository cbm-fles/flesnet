/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "device_operator.hpp"
#include "flib.h"
#include <iostream>

using namespace flib;

int main(int argc, char* argv[]) {

  bool report_flims = false;
  if (argc == 2 && std::string(argv[1]) == "--report-flims") {
    report_flims = true;
  }

  std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
  std::vector<std::unique_ptr<flib_device_flesin>> flibs;

  uint64_t num_dev = dev_op->device_count();

  std::cout << "Total number of FLIBs: " << num_dev << std::endl;

  for (size_t i = 0; i < num_dev; ++i) {
    std::cout << "*** FLIB " << i << " ***" << std::endl;
    try {
      flibs.push_back(
          std::unique_ptr<flib_device_flesin>(new flib_device_flesin(i)));
      std::cout << "Address: " << flibs.back()->print_devinfo() << std::endl;
      std::cout << "Hardware links: "
                << static_cast<unsigned>(flibs.back()->number_of_hw_links())
                << std::endl;
      std::cout << flibs.back()->print_build_info() << std::endl;

      if (report_flims) {
        // create flims for active links or internal pgen
        for (size_t j = 0; j < flibs.back()->number_of_links(); ++j) {
          auto rx_sel = flibs.back()->link(j)->data_sel();
          if (rx_sel == flib::flib_link::rx_link ||
              rx_sel == flib::flib_link::rx_pgen) {
            try {
              auto flim = std::unique_ptr<flib::flim>(
                  new flib::flim(flibs.back()->link(j)));
              std::cout << "** Link " << i << "/" << j << " **\n"
                        << "Data sel: " << rx_sel << "\n"
                        << flim->print_build_info() << std::endl;
            } catch (const std::exception& e) {
              std::cerr << "** Link " << i << "/" << j << " **\n"
                        << "Data sel: " << rx_sel << "\n"
                        << e.what() << std::endl;
            }
          }
        }
      }
    } catch (std::exception& e) {
      std::cout << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
