/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "cri.hpp"
#include "device_operator.hpp"
#include <iostream>
#include <memory>

using namespace cri;

int main() {

  int ret = EXIT_SUCCESS;

  try {
    std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
    std::vector<std::unique_ptr<cri_device>> cris;

    uint64_t num_dev = dev_op->device_count();

    std::cout << "Total number of CRIs: " << num_dev << std::endl;

    for (size_t i = 0; i < num_dev; ++i) {
      std::cout << "*** CRI " << i << " ***" << std::endl;
      try {
        cris.push_back(std::make_unique<cri_device>(i));
        std::cout << "Address: " << cris.back()->print_devinfo() << std::endl;
        std::cout << "Hardware channels: "
                  << static_cast<unsigned>(cris.back()->number_of_hw_channels())
                  << std::endl;
        std::cout << "Uptime: " << cris.back()->print_uptime() << std::endl;
        std::cout << cris.back()->print_build_info() << std::endl;
        std::string version_warning = cris.back()->print_version_warning();
        if (!version_warning.empty()) {
          std::cout << "*WARNING* " << version_warning << std::endl;
        }

      } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        ret = EXIT_FAILURE;
      }
    }

    return ret;

  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
