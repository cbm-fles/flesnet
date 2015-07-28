/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

// sends link local dlms

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

// int main(int argc, char* argv[]) {
int main() {

  MyFlib = new flib_device_flesin(0);
  size_t link = 0;

  MyFlib->link(link)->reset_datapath();

  uint32_t reg = MyFlib->link(link)->get_testreg();
  std::cout << "read  0x" << std::hex << reg << std::endl;

  uint32_t reg_wr = 0x1234ACBD;
  MyFlib->link(link)->set_testreg(reg_wr);
  std::cout << "write 0x" << std::hex << reg_wr << std::endl;

  reg = MyFlib->link(link)->get_testreg();
  std::cout << "read  0x" << std::hex << reg << std::endl;

  int ret = 0;
  if (reg_wr != reg) {
    ret = -1;
  }

  if (MyFlib)
    delete MyFlib;

  return ret;
}
