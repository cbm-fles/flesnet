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

#include "mc_functions.h"

using namespace flib;

flib_device* MyFlib = NULL;

int main(int argc, char *argv[])
{
  MyFlib = new flib_device(0);

  std::cout << MyFlib->print_build_info() << std::endl;
  std::cout << MyFlib->print_devinfo() << std::endl;

  //MyFlib->link(0).set_data_sel(flib_link::pgen);
    
  flib::flib_link::link_status_t status = MyFlib->link(0).get_link_status();
  
  std::stringstream ss;
  ss << "link status" << "\n"
     << "link_active " << status.link_active << "\n"
     << "data_rx_stop " << status.data_rx_stop << "\n"
     << "ctrl_rx_stop " << status.ctrl_rx_stop << "\n"
     << "ctrl_tx_stop " << status.ctrl_tx_stop << "\n";

  std::cout << ss.str();

  if (MyFlib)
    delete MyFlib;
  return 0;
}
