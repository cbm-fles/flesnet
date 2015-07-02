/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

/**
 * reads back cbmnet diagnostic counters for all active links
 */

#include <chrono>
#include "flib.h"

int main(int argc, char* argv[]) {
  int doreset = 0;

  if (argc == 2) {
    //    printf("Input: %i %s %s\n", argc, argv[0], argv[1]);
    //    printf("%s\n", argv[1]);

    std::string str1("-help"); // define help option
    if (str1.compare(argv[1]) == 0) {
      std::cout << "Usage: log_cbmnet [-reset]" << '\n';
      return -1;
    }

    std::string str2("-reset"); // define reset option
    if (str2.compare(argv[1]) == 0)
      doreset = 1; // will reset statistics

    //    if (doreset != 0)
    //    if (doreset)
    //      std::cout << "arg will reset" << '\n';
    //    printf("\n");
  }

  std::unique_ptr<flib::flib_device_cnet> flib =
      std::unique_ptr<flib::flib_device_cnet>(new flib::flib_device_cnet(0));
  std::vector<flib::flib_link_cnet*> flib_links = flib->links();

  // delete deactivated links from vector
  flib_links.erase(std::remove_if(std::begin(flib_links),
                                  std::end(flib_links),
                                  [](decltype(flib_links[0]) link) {
                     return link->data_sel() != flib::flib_link::rx_link;
                   }),
                   std::end(flib_links));

  std::cout << "enabled flib links detected: " << flib_links.size()
            << std::endl;

  std::cout << std::setw(14) << " time";
  for (auto link : flib_links) {
    // clear all counters
    if (doreset)
      link->diag_clear();
    size_t index = link->link_index();
    std::cout << std::setw(16) << "mc_index_" << index << std::setw(16)
              << "pcs_startup_" << index << std::setw(16) << "ebtb_code_err_"
              << index << std::setw(16) << "ebtb_disp_err_" << index
              << std::setw(16) << "crc_error_" << index << std::setw(16)
              << "packet_" << index << std::setw(16) << "packet_err_" << index
              << std::setw(16) << "loss_" << index;
  }
  std::cout << std::endl;

  // main loop
  while (true) {
    auto time = std::chrono::system_clock::now();
    std::cout << std::setw(14)
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     time.time_since_epoch()).count() << " ";
    for (auto link : flib_links) {
      auto mc_index = link->mc_index();
      auto pcs_startup = link->diag_pcs_startup();
      auto ebtb_code_err = link->diag_ebtb_code_err();
      auto ebtb_disp_err = link->diag_ebtb_disp_err();
      auto crc_error = link->diag_crc_error();
      auto packet = link->diag_packet();
      auto packet_err = link->diag_packet_err();
      double loss = (float)packet_err / (float)packet;

      std::cout << std::setw(16) << mc_index << std::setw(17) << pcs_startup
                << std::setw(17) << ebtb_code_err << std::setw(17)
                << ebtb_disp_err << std::setw(17) << crc_error << std::setw(17)
                << packet << std::setw(17) << packet_err << std::setw(17)
                << loss;
    }
    std::cout << std::endl;
    sleep(1);
  }
}
