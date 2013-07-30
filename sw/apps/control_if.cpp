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

#include <librorc.h>

#include "cbm_link.hpp"
#include "flib.hpp"
#include "mc_functions.h"

using namespace std;

flib* MyFlib = NULL;

int main(int argc, char *argv[])
{
  MyFlib = new flib(0);

  MyFlib->add_link(0);

  if(MyFlib->link[0]) {
    printf("link set\n");
  }
  else {
    printf("link not set\n");    
  };

  ////////////////////////////////////////////////////////

  printf("Firmware Date: %08x\n", MyFlib->get_reg(RORC_REG_FIRMWARE_DATE));
  printf("r_ctrl_tx: %08x\n", MyFlib->link[0]->get_ch()->getGTX(RORC_REG_GTX_CTRL_TX));

  MyFlib->link[0]->set_data_rx_sel(cbm_link::pgen);
    
  printf("START\n");

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

  if (MyFlib)
    delete MyFlib;
  return 0;
}
