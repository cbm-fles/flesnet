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

flib_device_cnet* MyFlib = NULL;

std::ostream& operator<<(std::ostream& os, flib_link::data_sel_t sel)
{
  switch(sel) {
  case flib_link::rx_disable : os << "disable"; break;
  case flib_link::rx_emu : os <<     "    emu"; break;
  case flib_link::rx_link : os <<    "   link"; break;
  case flib_link::rx_pgen : os <<    "   pgen"; break;
  default : os.setstate(std::ios_base::failbit);
  }
  return os;
}

int main(int argc, char *argv[])
{
  MyFlib = new flib_device_cnet(0);

  std::cout << MyFlib->print_build_info() << std::endl;
  std::cout << MyFlib->print_devinfo() << std::endl;

  size_t num_links = MyFlib->number_of_links();
  std::vector<flib_link_cnet*>links = MyFlib->links();

  std::stringstream ss;
  ss << "\nlink  data_sel  link_active  data_rx_stop  ctrl_rx_stop  ctrl_tx_stop \n";
  for( size_t i = 0; i < num_links; ++i) {
     ss << "   " << i
        << "   "<< links.at(i)->data_sel()
        << "            "<< links.at(i)->link_status().link_active
        << "             "<< links.at(i)->link_status().data_rx_stop
        << "             "<< links.at(i)->link_status().ctrl_rx_stop
        << "             "<< links.at(i)->link_status().ctrl_tx_stop
        << "\n";
  }
  std::cout << ss.str();

  if (MyFlib)
    delete MyFlib;
  return 0;
}
