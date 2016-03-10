/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <csignal>

#include <flib.h>

using namespace flib;

int s_interrupted = 0;
static void s_signal_handler(int signal_value) {
  (void)signal_value;
  s_interrupted = 1;
}

static void s_catch_signals(void) {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGABRT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);
}

//int main(int argc, char* argv[]) {
int main() {
  s_catch_signals();

  flib::flib_device_flesin flib(0);
  std::vector<flib::flib_link_flesin*> links = flib.links();
  std::vector<std::unique_ptr<flib::flim>> flims;

  // create flims for active links or internal pgen
  for (size_t i = 0; i < flib.number_of_links(); ++i) {
    auto rx_sel = links.at(i)->data_sel();
    if (rx_sel == flib::flib_link::rx_link ||
        rx_sel == flib::flib_link::rx_pgen) {
     try {
        flims.push_back(
            std::unique_ptr<flib::flim>(new flib::flim(links.at(i))));
      } catch (const std::exception& e) {
       std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
    }
  }

  if (flims.empty()) {
    std::cout << "No active links found" << std::endl;
    exit(EXIT_SUCCESS);
  }

  // reset at startup
  flib.enable_mc_cnt(false);
  for (auto&& flim : flims) {
    flim->set_pgen_enable(false);
    flim->set_ready_for_data(false);
    flim->reset_datapath();
    std::cout << flim->print_build_info() << std::endl;
    if (uint32_t mc_pend = flim->get_pgen_mc_pending() != 0) {
      std::cout << "*** ERROR *** mc pending (pgen) " << mc_pend << std::endl;
    }
  }

  std::cout << "enabling ..." << std::endl;

  for (auto&& flim : flims) {
    flim->set_pgen_start_time(100);
    flim->set_ready_for_data(true);
  }

  // enable pgen via master link 0
  flims.at(0)->set_pgen_enable(true);
  // enable flib internal pgen
  flib.enable_mc_cnt(true);

  std::cout << "running ..." << std::endl;

  while (s_interrupted == 0) {
    flims.at(0)->set_debug_out(false);
    ::sleep(1);
    flims.at(0)->set_debug_out(true);
    ::sleep(1);
  }
  
  std::cout << "disabling ..." << std::endl;

  flims.at(0)->set_pgen_enable(false);
  flib.enable_mc_cnt(false);
  for (auto&& flim : flims) {
    std::cout << "mc index (packer) " << flim->get_mc_idx() << std::endl;
    std::cout << "mc time (packer)  " << flim->get_mc_time() << std::endl;
    std::cout << "mc pending (pgen) " << flim->get_pgen_mc_pending()
              << std::endl;
  }

  for (auto&& flim : flims) {
    flim->set_ready_for_data(false);
    flim->reset_datapath();
    std::cout << "mc time (packer)  " << flim->get_mc_time() << std::endl;

    if (uint32_t mc_pend = flim->get_pgen_mc_pending() != 0) {
      std::cout << "*** ERROR *** mc pending (pgen) " << mc_pend << std::endl;
    }
  }

  std::cout << "Exiting" << std::endl;
  return 0;
}
