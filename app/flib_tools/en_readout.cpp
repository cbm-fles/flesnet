/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib.h"
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>
#include <thread>
#include <unistd.h>

using namespace flib;

int s_interrupted = 0;
static void s_signal_handler(int signal_value) {
  (void)signal_value;
  s_interrupted = 1;
}

static void s_catch_signals() {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGABRT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);
}

constexpr bool ext_pgen_sync = false;

int main(int argc, char* argv[]) {
  s_catch_signals();

  try {

    size_t flib_index = 0;
    if (argc >= 2) {
      flib_index = atoi(argv[1]);
    }
    std::cout << "using FLIB " << flib_index << std::endl;

    bool force_autobuild = false;
    if (argc == 3 && std::string(argv[2]) == "--archivable-data") {
      force_autobuild = true;
      std::cout << "Enforcing archivable data." << std::endl;
    }

    flib::flib_device_flesin flib(flib_index);
    std::vector<flib::flib_link_flesin*> links = flib.links();
    std::vector<std::unique_ptr<flib::flim>> flims;

    std::cout << flib.print_build_info() << std::endl;
    if (force_autobuild && flib.build_user() != "gitlab-runner") {
      throw std::runtime_error(
          "Unofficial FLIB build not suitable for archivable data.");
    }

    // create flims for active links or internal pgen
    for (size_t i = 0; i < flib.number_of_links(); ++i) {
      auto rx_sel = links.at(i)->data_sel();
      if (rx_sel == flib::flib_link::rx_link ||
          rx_sel == flib::flib_link::rx_pgen) {
        try {
          flims.push_back(
              std::unique_ptr<flib::flim>(new flib::flim(links.at(i))));
          std::cout << flims.back()->print_build_info() << std::endl;
          if (force_autobuild &&
              flims.back()->build_user() != "gitlab-runner") {
            throw std::runtime_error(
                "Unofficial FLIM build not suitable for archivable data.");
          }
        } catch (const std::exception& e) {
          std::cerr << e.what() << std::endl;
          return EXIT_FAILURE;
        }
      }
    }

    if (flims.empty()) {
      std::cout << "No active links found" << std::endl;
      return EXIT_SUCCESS;
    }

    // reset at startup
    flib.enable_mc_cnt(false);
    for (auto&& flim : flims) {
      flim->set_pgen_enable(false);
      flim->set_ready_for_data(false);
      flim->set_pgen_sync_ext(false);
      flim->reset_datapath();
      uint32_t mc_pend = flim->get_pgen_mc_pending();
      if (mc_pend != 0) {
        std::cout << "*** ERROR *** mc pending (pgen) " << mc_pend << std::endl;
      }
    }

    std::cout << "enabling ..." << std::endl;

    for (auto&& flim : flims) {
      flim->set_pgen_start_time(100);
      if (ext_pgen_sync) {
        flim->set_pgen_sync_ext(true);
      }
      flim->set_ready_for_data(true);
    }

    // enable pgens for all FLIMs
    // multi channel FLIMs will use channel 0 as master internally
    for (auto&& flim : flims) {
      flim->set_pgen_enable(true);
    }
    // enable flib internal pgen (global command for all channels)
    flib.enable_mc_cnt(true);

    std::cout << "running ..." << std::endl;

    while (s_interrupted == 0) {
      flims.at(0)->set_debug_out(false);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      flims.at(0)->set_debug_out(true);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "disabling ..." << std::endl;

    flims.at(0)->set_pgen_enable(false);
    flib.enable_mc_cnt(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    for (auto&& flim : flims) {
      std::cout << "mc pending (pgen) " << flim->get_pgen_mc_pending()
                << std::endl;
    }

    std::vector<uint64_t> mc_idx;
    std::vector<uint64_t> mc_time;

    for (auto&& flim : flims) {
      flim->set_ready_for_data(false);

      mc_idx.push_back(flim->get_mc_idx());
      mc_time.push_back(flim->get_mc_time());

      // index is incremented when ms is finished
      // time is captured with first word of ms
      // for correct stop index is one count ahead
      // ms size can be calculated (time - start_offset) / (index - 1)
      std::cout << "mc index (packer) " << mc_idx.back() << std::endl;
      std::cout << "mc time (packer)  " << mc_time.back() << std::endl;

      flim->reset_datapath();
      uint32_t mc_pend = flim->get_pgen_mc_pending();
      if (mc_pend != 0) {
        std::cout << "*** ERROR *** mc pending (pgen) " << mc_pend << std::endl;
      }
    }

    if (!(std::adjacent_find(mc_idx.begin(), mc_idx.end(),
                             std::not_equal_to<uint64_t>()) == mc_idx.end())) {
      std::cout << "*** WARNING *** mc index (packer) not equal" << std::endl;
    }
    if (!(std::adjacent_find(mc_time.begin(), mc_time.end(),
                             std::not_equal_to<uint64_t>()) == mc_time.end())) {
      std::cout << "*** WARNING *** mc time (packer) not equal" << std::endl;
    }

    std::cout << "Exiting" << std::endl;
  } catch (std::exception const& e) {
    std::cout << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
