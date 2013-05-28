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

#include "librorc.h"

#include "cbm_link.hpp"
#include "flib.hpp"
#include "mc_functions.h"

using namespace std;

flib* MyFlib = NULL;

void fnExit (void)
{
  if(MyFlib){
    delete MyFlib;
  }
  printf("Exiting\n");
}

static int s_interrupted = 0;
static void s_signal_handler (int signal_value)
{
  s_interrupted = 1;
  printf("Signal detected\n");
  exit(EXIT_SUCCESS);
}

static void s_catch_signals (void)
{
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset (&action.sa_mask);
  sigaction (SIGABRT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
  sigaction (SIGINT, &action, NULL);
}

int main(int argc, char *argv[])
{ 
  // register exit and signal handler
  atexit(fnExit);
  s_catch_signals();

  if(argc != 2) {
    printf("Usage: %s <#mc>\n", argv[0]);
    return -1;
  }

  int mc_limit = atoi(argv[1]);

  MyFlib = new flib(0);

  MyFlib->add_link(0);

  if(MyFlib->link[0]) {
    printf("link set\n");
  }
  else {
    printf("link not set\n");    
  };

  uint64_t* eb = (uint64_t *)MyFlib->link[0]->ebuf()->getMem();
  rb_entry* rb = (rb_entry *)MyFlib->link[0]->rbuf()->getMem();
  
  int error_cnt = 0;
  for (int j = 0; j < mc_limit; j++) {
    bool waited = false;

    std::pair<mc_desc, bool> mc_pair;
    while((mc_pair = MyFlib->link[0]->get_mc()).second == false ) {
      //      printf("waiting\n");
      usleep(1000);
      waited = true;
    }
    if(waited) {
      //printf(".\n");
      waited = false;
    }

    int error = process_mc(&mc_pair.first);
    error_cnt += error;
    if(error){
      dump_mc(&mc_pair.first);
      //      exit(EXIT_SUCCESS);
      printf("\n");
    }
    // dump_raw((uint64_t *)(rb+j), 4);
    // dump_mc(&mc_pair.first);

    MyFlib->link[0]->ack_mc();
      
    if((j & 0xFFFFFF) == 0xFFFFFF) {
      printf("%d analysed\n", j);
      //dump_mc(&mc_pair.first);
      // MyFlib->link[0]->ack_mc();
    }
    if (s_interrupted) {
      printf ("interrupt here received\n");
      break;
    }
  }
     
  delete MyFlib;
  MyFlib = nullptr;
  printf("MCs analysed %d\nTotal errors: %d\n", mc_limit, error_cnt);

  return 0;
}
