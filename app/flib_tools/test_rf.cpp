/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib.h"
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>

using namespace flib;

flib_device_flesin* MyFlib = NULL;
flim* MyFlim = NULL;

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

int main(int argc, char* argv[]) {
  s_catch_signals();

  size_t link = 0;
  if (argc == 2) {
    link = atoi(argv[1]);
    std::cout << "using link " << link << std::endl;
  } else {
    std::cout << "usage: " << argv[0] << " link" << std::endl;
    return -1;
  }

  MyFlib = new flib_device_flesin(0);
  int ret = 0;

  MyFlim = new flim(MyFlib->link(link));

  // reset at startup
  MyFlim->reset_datapath();

  std::cout << MyFlim->print_build_info() << std::endl;
  uint32_t mc_pend = MyFlim->get_pgen_mc_pending();
  if (mc_pend != 0) {
    std::cout << "ERROR: mc pending (pgen) " << mc_pend << std::endl;
  }

  uint32_t reg = 0;
  uint32_t reg_wr = 0;

  reg = MyFlim->get_testreg();
  std::cout << "read  0x" << std::hex << reg << std::endl;

  while (ret == 0 && s_interrupted == 0) {
    MyFlim->set_testreg(reg_wr);
    // std::cout << "write 0x" << std::hex << reg_wr << std::endl;

    reg = MyFlim->get_testreg();
    // std::cout << "read  0x" << std::hex << reg << std::endl;

    if (reg_wr % 500000 == 0) {
      std::cout << "written: " << reg_wr << std::endl;
    }

    if (reg_wr != reg) {
      std::cout << "write 0x" << std::hex << reg_wr << std::endl;
      std::cout << "read  0x" << std::hex << reg << std::endl;
      reg = MyFlim->get_testreg();
      std::cout << "read  0x" << std::hex << reg << std::endl;
      ret = -1;
    }
    ++reg_wr;
  }

  delete MyFlim;
  delete MyFlib;

  std::cout << "Exiting" << std::endl;
  return ret;
}
