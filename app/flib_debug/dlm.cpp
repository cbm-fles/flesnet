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
  unsigned int dlm = atoi(argv[1]);
  
  if (dlm > 15) {printf("./dlm num with 0 <= num <= 15\n"); return -1;}
      
  MyFlib = new flib_device(0);

  //MyFlib->get_link(0).set_data_rx_sel(flib_link::link);

  MyFlib->get_link(0).prepare_dlm(dlm, true);
  MyFlib->get_link(0).send_dlm();

  if (MyFlib) delete MyFlib;
  
  return 0;
}
