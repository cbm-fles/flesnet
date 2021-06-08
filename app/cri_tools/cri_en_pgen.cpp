/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "device_operator.hpp"
#include "cri.hpp"
#include <iostream>

using namespace cri;

int main(int argc, char* argv[]) {

  bool enable = false;
  if (argc >= 2) {
      enable = atoi(argv[1]);
  }
  
  std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
  std::vector<std::unique_ptr<cri_device>> cris;

  uint64_t num_dev = dev_op->device_count();
  
  std::cout << "Total number of CRIs: " << num_dev << std::endl;

  for (size_t i = 0; i < num_dev; ++i) {
    std::cout << "*** CRI " << i << " ***" << std::endl;
    try {
      cris.push_back(
          std::unique_ptr<cri_device>(new cri_device(i)));
      std::cout << "Address: " << cris.back()->print_devinfo() << std::endl;
      if (enable == true) {
        std::cout << "enabling pgen ... " << std::endl;
        cris.back()->enable_mc_cnt(true);
      } else {
        std::cout << "disabling pgen ... " << std::endl;
        cris.back()->enable_mc_cnt(false);
      }
    } catch (std::exception& e) {
      std::cout << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
