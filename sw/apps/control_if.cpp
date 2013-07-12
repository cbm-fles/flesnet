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
  if(argc == 2) {
    size_t arg = atoi(argv[1]);
    if (arg < 4 || arg > 32) {
      printf("wrong argument\n");
    }
    else
      words = arg;
  }

  uint16_t* msg = (uint16_t*)malloc(words<<1);
  uint16_t* buf = (uint16_t*)malloc(64);

  // receive to flush hw buffers
  if ( MyFlib->link[0]->rcv_msg(buf) < 0)  {
    printf("nothing to receive\n");
  }
  
  // fill buffer
  for (size_t i = 0; i < words; i++) {
    msg[i] = i;
  }

  if ( MyFlib->link[0]->send_msg(msg, words) < 0)  {
    printf("sending failed\n");
  }
    
  size_t words_rcvd = 0;
  // receive msg
  if ( (words_rcvd = MyFlib->link[0]->rcv_msg(buf)) < 0)  {
    printf("nothing to receive\n");
  }

  for (size_t i = 0; i < words_rcvd; i++) {
    printf("buf: %04x\n", buf[i]);
  }

  free(msg);
  free(buf);

  if (MyFlib)
    delete MyFlib;
  return 0;
}
