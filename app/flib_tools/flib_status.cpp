/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <iostream>

#include <flib.h>
#include <device_operator.hpp>

using namespace flib;

int main() {

  std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
  std::vector<std::unique_ptr<flib_device_flesin>> flibs;

  uint64_t num_dev = dev_op->device_count();

  std::cout << "Total number of FLIBs: " << num_dev << std::endl;

  for (size_t i = 0; i < num_dev; ++i) {
    flibs.push_back(
        std::unique_ptr<flib_device_flesin>(new flib_device_flesin(i)));

    std::cout << "*** FLIB " << i << " ***" << std::endl;
    std::cout << flibs.back()->print_build_info() << std::endl;
    std::cout << flibs.back()->print_devinfo() << std::endl;
    std::cout << "Hardware links: "
              << static_cast<unsigned>(flibs.back()->number_of_hw_links())
              << std::endl;
  }

  return 0;
}
