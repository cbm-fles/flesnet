/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "cri.hpp"
#include <cerrno>
#include <chrono>
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

using namespace cri;

cri_device* Cri = NULL;

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

int main() {
  s_catch_signals();

  Cri = new cri_device(0);
  int ret = 0;

  uint32_t reg = 0;
  uint32_t reg_wr = 0;

  reg = Cri->get_testreg();
  std::cout << "read  0x" << std::hex << reg << std::endl;

  auto start = std::chrono::high_resolution_clock::now();
  std::chrono::time_point<std::chrono::high_resolution_clock> end;

  while (ret == 0 && s_interrupted == 0) {
    Cri->set_testreg(reg_wr);
    // std::cout << "write 0x" << std::hex << reg_wr << std::endl;

    reg = Cri->get_testreg();
    // std::cout << "read  0x" << std::hex << reg << std::endl;

    if (reg_wr % 1000000 == 0) {
      end = std::chrono::high_resolution_clock::now();
      std::cout << "written: " << std::dec << reg_wr / 1000000 << " M"
                << std::endl;
      std::chrono::duration<double> duration = (end - start);
      std::cout << "Write-read roundtrip " << std::dec
                << std::chrono::duration_cast<std::chrono::nanoseconds>(
                       duration)
                           .count() /
                       1000000
                << " ns." << std::endl;
      start = std::chrono::high_resolution_clock::now();
    }

    if (reg_wr != reg) {
      std::cout << "write 0x" << std::hex << reg_wr << std::endl;
      std::cout << "read  0x" << std::hex << reg << std::endl;
      reg = Cri->get_testreg();
      std::cout << "read  0x" << std::hex << reg << std::endl;
      ret = -1;
    }
    ++reg_wr;
  }

  delete Cri;

  std::cout << "Exiting" << std::endl;
  return ret;
}
