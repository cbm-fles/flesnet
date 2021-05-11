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
#include <random>
#include <sys/time.h>
#include <unistd.h>

using namespace cri;

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

  std::unique_ptr<cri_device> Cri(new cri_device(0));

  int ret = 0;

  std::random_device seed;
  std::default_random_engine engine(seed());
  std::uniform_int_distribution<unsigned int> uniform_dist(0);
  uint32_t reg_wr = uniform_dist(engine);

  uint32_t reg = 0;
  reg = Cri->get_testreg();
  std::cout << "device read  0x" << std::hex << reg << std::endl;

  uint32_t dma_reg = 0;
  dma_reg = Cri->link(0)->get_testreg_dma();
  std::cout << "dma read     0x" << std::hex << dma_reg << std::endl;

  uint32_t data_reg = 0;
  data_reg = Cri->link(0)->get_testreg_data();
  std::cout << "data read    0x" << std::hex << data_reg << std::endl;

  auto start = std::chrono::high_resolution_clock::now();
  std::chrono::time_point<std::chrono::high_resolution_clock> end;

  size_t it = 0;
  while (ret == 0 && s_interrupted == 0) {
    Cri->set_testreg(reg_wr);
    // std::cout << "write 0x" << std::hex << reg_wr << std::endl;
    Cri->link(0)->set_testreg_dma(reg_wr << 1);
    Cri->link(0)->set_testreg_data(reg_wr << 2);

    reg = Cri->get_testreg();
    // std::cout << "read  0x" << std::hex << reg << std::endl;
    dma_reg = Cri->link(0)->get_testreg_dma();
    data_reg = Cri->link(0)->get_testreg_data();

    if (it % 1000000 == 0) {
      end = std::chrono::high_resolution_clock::now();
      std::cout << "written: " << std::dec << it / 1000000 << " M" << std::endl;
      std::chrono::duration<double> duration = (end - start);
      std::cout << "Write-read roundtrip " << std::dec
                << std::chrono::duration_cast<std::chrono::nanoseconds>(
                       duration)
                           .count() /
                       (1000000 * 3)
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
    if ((reg_wr << 1) != dma_reg) {
      std::cout << "write 0x" << std::hex << (reg_wr << 1) << std::endl;
      std::cout << "read  0x" << std::hex << dma_reg << std::endl;
      dma_reg = Cri->link(0)->get_testreg_dma();
      std::cout << "read  0x" << std::hex << dma_reg << std::endl;
      ret = -1;
    }
    if ((reg_wr << 2) != data_reg) {
      std::cout << "write 0x" << std::hex << (reg_wr << 2) << std::endl;
      std::cout << "read  0x" << std::hex << data_reg << std::endl;
      data_reg = Cri->link(0)->get_testreg_data();
      std::cout << "read  0x" << std::hex << data_reg << std::endl;
      ret = -1;
    }
    reg_wr = uniform_dist(engine);
    ++it;
  }

  std::cout << "Exiting" << std::endl;
  return ret;
}
