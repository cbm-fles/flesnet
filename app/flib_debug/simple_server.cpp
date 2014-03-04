// Simpel example application initializing a FLIB for date transfer

#include <iostream>
#include <csignal>
#include <unistd.h>

#include <flib.h>
#include "global.hpp"

#include "mc_functions.h"

int s_interrupted = 0;
static void s_signal_handler (int signal_value)
{
  s_interrupted = 1;
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

einhard::Logger<(einhard::LogLevel) MINLOGLEVEL, true> out(einhard::WARN, true);

int main(int argc, const char* argv[])
{
  s_catch_signals();

  out.setVerbosity(einhard::DEBUG);

  // create FLIB
  try 
  {
  flib::flib_device flib(0);

  std::vector<flib::flib_link*> links = flib.get_links();

  out.info() << flib.print_build_info();

  // initialize a DMA buffer
  links.at(0)->init_dma(flib::create_only, 22, 20);
  out.info() << "Event Buffer: " << links.at(0)->get_ebuf_info();
  out.info() << "Desciptor Buffer" << links.at(0)->get_dbuf_info();
  // get raw pointers for debugging
  uint64_t* eb = (uint64_t *)links.at(0)->ebuf()->getMem();
  MicrosliceDescriptor* rb = (MicrosliceDescriptor *)links.at(0)->rbuf()->getMem();

  // set start index
  links.at(0)->set_start_idx(1);

  // set the aproriate header config
  flib::hdr_config config = {};
  config.eq_id = 0xE003;
  config.sys_id = 0xBC;
  config.sys_ver = 0xFD;
  links.at(0)->set_hdr_config(&config);

  // enable data source
  bool use_sp = false; 
  if (use_sp == true) {
    links.at(0)->set_data_rx_sel(flib::flib_link::link);
    links.at(0)->set_dlm_cfg(0x8, true); // enable DAQ
    flib.send_dlm();
  } 
  else {
    links.at(0)->set_data_rx_sel(flib::flib_link::pgen);
  }
 
  flib.enable_mc_cnt(true);
  links.at(0)->enable_cbmnet_packer(true);

  out.debug() << "current mc nr: " <<  links.at(0)->get_mc_index();

  /////////// THE MAIN LOOP ///////////

  size_t pending_acks = 0;
  size_t j = 0;
  
  while(s_interrupted==0) {
    std::pair<flib::mc_desc, bool> mc_pair;
    while ((mc_pair = links.at(0)->get_mc()).second == false && s_interrupted==0) {
      usleep(10);
      links.at(0)->ack_mc();
      pending_acks = 0;
    }
    pending_acks++;
 
    if (s_interrupted != 0) {
      break;
    }

    if (j == 0) {
      out.info() << "First MC seen.";
      dump_mc_light(&mc_pair.first);
      std::cout << "RB:" << std::endl;
      dump_raw((uint64_t *)(rb+j), 8);
      std::cout << "EB:" << std::endl;
      dump_raw((uint64_t *)(eb), 64);
    }
 
    if (j != 0 && j < 1) {
      out.info() << "MC analysed " << j;
      dump_mc_light(&mc_pair.first);
      dump_mc(&mc_pair.first);
    }
  
    if ((j & 0xFFFFF) == 0xFFFFF) {
      out.info() << "MC analysed " << j;
      dump_mc_light(&mc_pair.first);
      dump_mc(&mc_pair.first);
    }
 
    if (pending_acks == 10) {
      links.at(0)->ack_mc();
      pending_acks = 0;
    }
 
    j++;
  }
  
  // disable data source
  flib.enable_mc_cnt(false);
  out.debug() << "current mc nr 0x: " << std::hex <<  links.at(0)->get_mc_index();
  out.debug() << "pending mc: "  << links.at(0)->get_pending_mc();
  out.debug() << "busy: " <<  links.at(0)->get_ch()->getDMABusy();

  out.debug() << "Exiting";
  
    }
  catch (std::exception& e) 
    {
      out.error() << e.what();
    }
  return 0;
}
