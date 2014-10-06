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
  size_t link_nr = atoi(argv[1]);
  
  if (link_nr < 8) {printf("./link number invalid\n"); return -1;}
      
  MyFlib = new flib_device(0);

  MyFlib->link(0).rst_cnet_link();

  if (MyFlib) delete MyFlib;
  
  return 0;
}
