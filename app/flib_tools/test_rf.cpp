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

flib_device_flesin* MyFlib = NULL;

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

// int main(int argc, char* argv[]) {
int main() {
  s_catch_signals();

  MyFlib = new flib_device_flesin(0);
  size_t link = 0;
  int ret = 0;

  //  uint32_t reg = 0;
  //  uint32_t reg_wr = 0x12340000;

  if (MyFlib->link(link)->flim_hardware_id() != 0x4844 ||
      MyFlib->link(link)->flim_hardware_id() != 2) {
    std::cerr << "FLIM not reachable" << std::endl;
  }

  // reset at startup
  MyFlib->link(link)->set_flim_pgen_enable(false);
  MyFlib->link(link)->set_flim_ready_for_data(false);
  MyFlib->link(link)->reset_flim_pgen_mc_pending();

  std::cout << MyFlib->link(link)->print_flim_build_info() << std::endl;
  std::cout << "mc index: " << MyFlib->link(link)->get_flim_mc_idx()
            << std::endl;
  std::cout << "mc pending " << MyFlib->link(link)->get_flim_pgen_mc_pending()
            << std::endl;

  // MyFlib->link(link)->reset_datapath();

  // reg = MyFlib->link(link)->get_testreg();
  // std::cout << "read  0x" << std::hex << reg << std::endl;

  //  while (ret == 0) {
  //    MyFlib->link(link)->set_testreg(reg_wr);
  //    std::cout << "write 0x" << std::hex << reg_wr << std::endl;
  //
  //    reg = MyFlib->link(link)->get_testreg();
  //    std::cout << "read  0x" << std::hex << reg << std::endl;
  //
  //    if (reg_wr != reg) {
  //      ret = -1;
  //    }
  //    ++reg_wr;
  //  }

  MyFlib->link(link)->set_flim_ready_for_data(true);
  MyFlib->link(link)->set_flim_pgen_enable(true);

  while (s_interrupted == 0) {
    MyFlib->link(link)->set_flim_debug_out(false);
    ::sleep(1);
    MyFlib->link(link)->set_flim_debug_out(true);
    ::sleep(1);
  }
  //  while (s_interrupted == 0) {
  //    ::sleep(1);
  //  }

  std::cout << "mc index: " << MyFlib->link(link)->get_flim_mc_idx()
            << std::endl;
  std::cout << "mc pending " << MyFlib->link(link)->get_flim_pgen_mc_pending()
            << std::endl;
  MyFlib->link(link)->set_flim_pgen_enable(false);
  MyFlib->link(link)->set_flim_ready_for_data(false);
  MyFlib->link(link)->reset_flim_pgen_mc_pending();

  if (MyFlib)
    delete MyFlib;

  std::cout << "Exiting" << std::endl;
  return ret;
}
