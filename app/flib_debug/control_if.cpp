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

  ////////////////////////////////////////////////////////
 
  std::cout << MyFlib->print_build_info() << std::endl;

  printf("r_ctrl_tx: %08x\n", MyFlib->get_link(0).get_rfgtx()->get_reg(RORC_REG_GTX_CTRL_TX));

  MyFlib->get_link(0).set_data_rx_sel(flib_link::rx_pgen);
    
  printf("START\n");

  std::cout << MyFlib->print_devinfo() << std::endl;

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
  if ( MyFlib->get_link(0).recv_dcm(&r_msg) < 0)  {
    printf("nothing to receive\n");
  }
  

  if ( MyFlib->get_link(0).send_dcm(&s_msg) < 0)  {
    printf("sending failed\n");
  }                      
    
  // receive msg
  if ( MyFlib->get_link(0).recv_dcm(&r_msg) < 0)  {
    printf("error receiving\n");
  }

  for (size_t i = 0; i < r_msg.words; i++) {
    printf("buf: 0x%04x\n", r_msg.data[i]);
  }

  printf("dlm: %01x\n", MyFlib->get_link(0).recv_dlm());
  MyFlib->get_link(0).prepare_dlm(0x5, true);
  MyFlib->get_link(0).send_dlm();
  printf("dlm: %01x\n", MyFlib->get_link(0).recv_dlm());
  printf("dlm: %01x\n", MyFlib->get_link(0).recv_dlm());
  MyFlib->get_link(0).prepare_dlm(0x2, false);

  printf("mc_index: %01lx\n", MyFlib->get_link(0).get_mc_index());
  MyFlib->get_link(0).set_start_idx(5);
  
  printf("mc_index: %01lx\n", MyFlib->get_link(0).get_mc_index());


  if (MyFlib)
    delete MyFlib;
  return 0;
}
