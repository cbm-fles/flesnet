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

  if(MyFlib->link[0]) {
    printf("link set\n");
  }
  else {
    printf("link not set\n");    
  };

  ////////////////////////////////////////////////////////
 
  flib_device::build_info build = MyFlib->get_build_info();
  std::cout << "Build Date:     " << build.date << std::endl;
  std::cout << "Build Revision: " << std::hex \
            << build.rev[4] << build.rev[3] << build.rev[2] \
            << build.rev[1] << build.rev[0] << std::endl;
  if (build.clean)
    std::cout << "Repository was clean " << std::endl;
  else
    std::cout << "Repository was not clean " << std::endl;

  std::cout << MyFlib->print_build_info();

printf("r_ctrl_tx: %08x\n", MyFlib->link[0]->get_ch()->getGTX(RORC_REG_GTX_CTRL_TX));

  MyFlib->link[0]->set_data_rx_sel(flib_link::pgen);
    
  printf("START\n");

  std::cout << MyFlib->get_devinfo();

  size_t words = 4; // number of 16 Bit words in message
  uint16_t offset = 0;
  if(argc == 3) {
    size_t arg = atoi(argv[1]);
    if (arg < 4 || arg > 32) {
      printf("wrong argument\n");
    }
    else
      words = arg;
    offset = atoi(argv[2]);
  }

  ctrl_msg s_msg;
  ctrl_msg r_msg;

  // fill message
  for (size_t i = 0; i < 32; i++) {
    s_msg.data[i] = offset+i;
  }
  s_msg.words = words;


  // receive to flush hw buffers
  if ( MyFlib->link[0]->rcv_msg(&r_msg) < 0)  {
    printf("nothing to receive\n");
  }
  

  if ( MyFlib->link[0]->send_msg(&s_msg) < 0)  {
    printf("sending failed\n");
  }                      
    
  // receive msg
  if ( MyFlib->link[0]->rcv_msg(&r_msg) < 0)  {
    printf("error receiving\n");
  }

  for (size_t i = 0; i < r_msg.words; i++) {
    printf("buf: 0x%04x\n", r_msg.data[i]);
  }

  printf("dlm: %01x\n", MyFlib->link[0]->get_dlm());
  MyFlib->link[0]->set_dlm_cfg(0x5, true);
  MyFlib->send_dlm();
  printf("dlm: %01x\n", MyFlib->link[0]->get_dlm());
  MyFlib->link[0]->clr_dlm();
  printf("dlm: %01x\n", MyFlib->link[0]->get_dlm());
  MyFlib->link[0]->set_dlm_cfg(0x2, false);

  printf("mc_index: %01lx\n", MyFlib->link[0]->get_mc_index());
  MyFlib->link[0]->set_start_idx(5);
  
  printf("mc_index: %01lx\n", MyFlib->link[0]->get_mc_index());


  if (MyFlib)
    delete MyFlib;
  return 0;
}
