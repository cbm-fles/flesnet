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
#include <syslog.h>

#include "librorc.h"

#include "cbm_link.hpp"
#include "flib.hpp"
#include "mc_functions.h"

using namespace std;

flib* MyFlib = NULL;

void fnExit (void)
{
  // disable mc_gen
  MyFlib->enable_mc_cnt(false);
  // reset mc pending‚
  MyFlib->link[0]->rst_pending_mc();

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

  openlog ("flib_control", LOG_PERROR, LOG_LOCAL1);

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
  uint32_t pending_acks = 0;

  hdr_config config = {};
  config.eq_id = 0xE003;
  config.sys_id = 0xBC;
  config.sys_ver = 0xFD;
  MyFlib->link[0]->set_hdr_config(&config);
 

  syslog(LOG_NOTICE, "misc mc_gen cfg: %08x\n", MyFlib->link[0]->get_ch()->getGTX(RORC_REG_GTX_MC_GEN_CFG));
  syslog(LOG_NOTICE, "misc datapath cfg: %08x\n", MyFlib->link[0]->get_ch()->getGTX(RORC_REG_GTX_DATAPATH_CFG));
  syslog(LOG_NOTICE, "pending mc: %016lx\n", MyFlib->link[0]->get_pending_mc());
  
  MyFlib->link[0]->set_data_rx_sel(cbm_link::pgen);
  MyFlib->link[0]->enable_cbmnet_packer(true);

  // enable mc gen
  MyFlib->enable_mc_cnt(true);
 
  syslog(LOG_NOTICE, "misc datapath cfg: %08x\n", MyFlib->link[0]->get_ch()->getGTX(RORC_REG_GTX_DATAPATH_CFG));

  int error_cnt = 0;
  size_t j = 0;
  
  while(1) {  
    //for (int j = 0; j < mc_limit; j++) {
    bool waited = false;

    std::pair<mc_desc, bool> mc_pair;
    while ((mc_pair = MyFlib->link[0]->get_mc()).second == false ) {
      //printf("waiting\n");
      usleep(10);
      MyFlib->link[0]->ack_mc();
      pending_acks = 0;
      waited = true;
    }
    pending_acks++;
    if (waited) {
      //printf(".\n");
      waited = false;
    }
    if (j == 0) {
      printf("First mc seen\n");
      dump_raw((uint64_t *)(rb+j), 4);
      dump_mc_light(&mc_pair.first);
    }
    int error = process_mc(&mc_pair.first);
    error_cnt = error;
    if (error != 0 && j != 0){
      //dump_raw((uint64_t *)(rb+j), 4);
      //dump_mc(&mc_pair.first);
      //exit(EXIT_SUCCESS);
      //printf("\n");
      break;
    }
    //dump_report((rb_entry*)(mc_pair.first.rbaddr));
    //dump_mc(&mc_pair.first);

    if ((j & 0x3FFFFFF) == 0x3FFFFFF) {
      syslog(LOG_INFO, "%ld analysed\n", j);
      //      dump_mc_light(&mc_pair.first);
    }

    if (pending_acks == 100) {
      MyFlib->link[0]->ack_mc();
      pending_acks = 0;
    }

    if (s_interrupted) {
      break;
    }
    j++;
  }

  // disable mc_gen
  MyFlib->enable_mc_cnt(false);
  syslog(LOG_NOTICE, "pending mc : %016lx\n", MyFlib->link[0]->get_pending_mc());
  // reset mc pending‚
  MyFlib->link[0]->rst_pending_mc();

  delete MyFlib;
  MyFlib = nullptr;
  syslog(LOG_NOTICE, "MCs analysed %ld, Total errors: %d\n", j, error_cnt);

  return 0;
}
