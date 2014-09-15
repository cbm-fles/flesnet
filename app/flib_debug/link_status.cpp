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

std::ostream& operator<<(std::ostream& os, flib_link::data_rx_sel sel)
{
  switch(sel) {
  case flib_link::disable : os << "disable"; break;
  case flib_link::emu : os <<     "    emu"; break;
  case flib_link::link : os <<    "   link"; break;
  case flib_link::pgen : os <<    "   pgen"; break;
  default : os.setstate(std::ios_base::failbit);
  }
  return os;
}

int main(int argc, char *argv[])
{
  MyFlib = new flib_device(0);

  std::cout << MyFlib->print_build_info() << std::endl;
  std::cout << MyFlib->print_devinfo() << std::endl;

  size_t num_links = MyFlib->get_num_links();
  std::vector<flib_link*>links = MyFlib->get_links();

  std::stringstream ss;
  ss << "\nlink  data_sel  link_active  data_rx_stop  ctrl_rx_stop  ctrl_tx_stop \n";
  for( size_t i = 0; i < num_links; ++i) {
     ss << "   " << i
        << "   "<< links.at(i)->get_data_rx_sel()
        << "            "<< links.at(i)->get_link_status().link_active
        << "             "<< links.at(i)->get_link_status().data_rx_stop
        << "             "<< links.at(i)->get_link_status().ctrl_rx_stop
        << "             "<< links.at(i)->get_link_status().ctrl_tx_stop
        << "\n";
  }
  std::cout << ss.str();

  if (MyFlib)
    delete MyFlib;
  return 0;
}
