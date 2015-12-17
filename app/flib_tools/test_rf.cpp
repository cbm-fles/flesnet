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
flim* MyFlim = NULL;

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

  MyFlim = new flim(MyFlib->link(link));

  // reset at startup
  MyFlim->set_pgen_enable(false);
  MyFlim->set_ready_for_data(false);
  MyFlim->reset_pgen_mc_pending();
  MyFlim->set_start_idx(0);

  std::cout << MyFlim->print_build_info() << std::endl;
  if (uint32_t mc_idx = MyFlim->get_mc_idx() != 0) {
    std::cout << "ERROR: mc index (packer) " << mc_idx << std::endl;
  }
  if (uint32_t mc_pend = MyFlim->get_pgen_mc_pending() != 0) {
    std::cout << "ERROR: mc pending (pgen) " << mc_pend << std::endl;
  }
  // MyFlib->link(link)->reset_datapath();

  // reg = MyFlim->get_testreg();
  // std::cout << "read  0x" << std::hex << reg << std::endl;

  //  while (ret == 0) {
  //    MyFlim->set_testreg(reg_wr);
  //    std::cout << "write 0x" << std::hex << reg_wr << std::endl;
  //
  //    reg = MyFlim->get_testreg();
  //    std::cout << "read  0x" << std::hex << reg << std::endl;
  //
  //    if (reg_wr != reg) {
  //      ret = -1;
  //    }
  //    ++reg_wr;
  //  }

  std::cout << "enabling ..." << std::endl;
  MyFlim->set_ready_for_data(true);
  MyFlim->set_pgen_enable(true);

  std::cout << "running ..." << std::endl;
  while (s_interrupted == 0) {
    MyFlim->set_debug_out(false);
    ::sleep(1);
    MyFlim->set_debug_out(true);
    ::sleep(1);
  }
  //  while (s_interrupted == 0) {
  //    ::sleep(1);
  //  }

  std::cout << "mc index (packer) " << MyFlim->get_mc_idx() << std::endl;
  std::cout << "mc pending (pgen) " << MyFlim->get_pgen_mc_pending()
            << std::endl;
  std::cout << "disabling ..." << std::endl;
  MyFlim->set_pgen_enable(false);
  MyFlim->set_ready_for_data(false);
  MyFlim->reset_pgen_mc_pending();
  MyFlim->set_start_idx(0);
  if (uint32_t mc_idx = MyFlim->get_mc_idx() != 0) {
    std::cout << "ERROR: mc index (packer) " << mc_idx << std::endl;
  }
  if (uint32_t mc_pend = MyFlim->get_pgen_mc_pending() != 0) {
    std::cout << "ERROR: mc pending (pgen) " << mc_pend << std::endl;
  }

  if (MyFlim)
    delete MyFlim;

  if (MyFlib)
    delete MyFlib;

  std::cout << "Exiting" << std::endl;
  return ret;
}
