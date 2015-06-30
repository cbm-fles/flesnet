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


int main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Usage: link_reset <link number>\n");
    return -1;
  }
  size_t link_nr = atoi(argv[1]);
  
  if (link_nr > 7) {
    printf("Invalid link number (valid links: 0..7)\n");
    return -1;
  }
      
  MyFlib = new flib_device_cnet(0);

  printf("reseting CBMnet link %lu\n", link_nr);
  MyFlib->link(link_nr)->rst_cnet_link();

  if (MyFlib) delete MyFlib;
  
  return 0;
}
