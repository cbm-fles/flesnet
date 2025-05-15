/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "cri.hpp"
#include "device_operator.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

using namespace cri;

int main(int argc, char* argv[]) {

  bool enable = false;
  if (argc >= 2) {
    enable = (atoi(argv[1]) != 0);
  }

  std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
  std::vector<std::unique_ptr<cri_device>> cris;
  uint64_t num_dev = dev_op->device_count();
  std::vector<uint32_t> ms_sizes(num_dev);
  std::vector<uint64_t> start_times(num_dev);

  // Create all CRI devices
  std::cout << "Total number of CRIs: " << num_dev << std::endl;
  for (size_t i = 0; i < num_dev; ++i) {
    std::cout << "CRI " << i;
    try {
      cris.push_back(std::make_unique<cri_device>(i));
      std::cout << ": address " << cris.back()->print_devinfo() << std::endl;
      if (enable) {
        ms_sizes.at(i) = cris.back()->get_pgen_mc_size_ns();
      }
    } catch (std::exception& e) {
      std::cout << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  // Calulate the start time for each device
  // The start time is the system time rounded to microslice size
  if (enable) {
    uint64_t now =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    std::cout << "System time: " << now << std::endl;
    for (size_t i = 0; i < num_dev; ++i) {
      start_times.at(i) = (now / ms_sizes.at(i)) * ms_sizes.at(i);
      std::cout << "CRI 0: pgen microslice size: " << ms_sizes.at(i) << " ns \n"
                << "CRI 0: pgen start time: " << start_times.at(i) << std::endl;
    }
  }

  // Enable/disable pgen
  for (size_t i = 0; i < num_dev; ++i) {
    if (enable) {
      std::cout << "CRI " << i << ": enabling pgen ... " << std::endl;
      cris.at(i)->set_pgen_start_time(start_times.at(i));
      cris.at(i)->enable_mc_cnt(true);
    } else {
      std::cout << "CRI " << i << ": disabling pgen ... " << std::endl;
      cris.at(i)->enable_mc_cnt(false);
    }
  }

  return EXIT_SUCCESS;
}
