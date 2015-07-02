// sends link local dlms

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

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("./dlm link num\n");
    return -1;
  }

  unsigned int link = atoi(argv[1]);
  if (link > 7) {
    printf("./dlm link num with 0 <= link <= 7\n");
    return -1;
  }

  unsigned int dlm = atoi(argv[2]);
  if (dlm > 15) {
    printf("./dlm link num with 0 <= num <= 15\n");
    return -1;
  }

  MyFlib = new flib_device_cnet(0);

  MyFlib->link(link)->prepare_dlm(dlm, true);
  MyFlib->link(link)->send_dlm();

  if (MyFlib)
    delete MyFlib;

  return 0;
}
