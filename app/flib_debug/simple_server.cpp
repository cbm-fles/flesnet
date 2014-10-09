// Simpel example application initializing a FLIB for date transfer

#include <iostream>
#include <csignal>
#include <unistd.h>

#include <flib.h>
#include <log.hpp>

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


int main(int argc, const char* argv[])
{
  s_catch_signals();

  logging::add_console(debug);

  // create FLIB
  try 
  {
  flib::flib_device flib(0);

  std::vector<flib::flib_link*> links = flib.links();

  L_(info) << flib.print_build_info();

  flib.set_mc_time(500);

  std::vector<uint64_t*> eb;
  std::vector<MicrosliceDescriptor*> rb;

  for (size_t i = 0; i < flib.number_of_links(); ++i) {
    L_(debug) << "initializing link " << i;

    // initialize a DMA buffer
    links.at(i)->init_dma(flib::create_only, 22, 20);
    L_(info) << "Event Buffer: " << links.at(i)->data_buffer_info();
    L_(info) << "Desciptor Buffer" << links.at(i)->desc_buffer_info();
    // get raw pointers for debugging
    eb.push_back((uint64_t *)links.at(i)->data_buffer()->mem());
    rb.push_back((MicrosliceDescriptor *)links.at(i)->desc_buffer()->mem());
    
    // set start index
    links.at(i)->set_start_idx(1);
    
    // set the aproriate header config
    hdr_config config = {};
    config.eq_id = 0xE003;
    config.sys_id = 0xBC;
    config.sys_ver = 0xFD;
    links.at(i)->set_hdr_config(&config);
    
    // enable data source
    bool use_sp = false; 
    if (use_sp == true) {
      links.at(i)->set_data_sel(flib::flib_link::rx_link);
      links.at(i)->prepare_dlm(0x8, true); // enable DAQ
      links.at(i)->send_dlm();
    } 
    else {
      links.at(i)->set_data_sel(flib::flib_link::rx_pgen);
    }
    
    flib.enable_mc_cnt(true);
    links.at(i)->enable_cbmnet_packer(true);
    
    L_(debug) << "current mc nr: " <<  links.at(i)->mc_index();
    L_(debug) << "link " << i << "initialized";
  }
  /////////// THE MAIN LOOP ///////////

  size_t pending_acks = 0;
  size_t j = 0;
  size_t i = 0; // link number

  size_t data_size = 0;
  size_t mc_received = 0;

  while(s_interrupted==0) {
    std::pair<flib::mc_desc, bool> mc_pair;
    while ((mc_pair = links.at(i)->mc()).second == false && s_interrupted==0) {
      usleep(10);
      links.at(i)->ack_mc();
      pending_acks = 0;
    }
    pending_acks++;
 
    if (s_interrupted != 0) {
      break;
    }

    data_size += mc_pair.first.size;
    ++mc_received;

    if (j == 0) {
      L_(info) << "First MC seen.";
      dump_mc_light(&mc_pair.first);
      std::cout << "RB:" << std::endl;
      dump_raw((uint64_t *)(rb.at(i)+j), 8);
      std::cout << "EB:" << std::endl;
      dump_raw((uint64_t *)(eb.at(i)), 64);
    }
 
    if (j != 0 && j < 1) {
      L_(info) << "MC analysed " << j;
      dump_mc_light(&mc_pair.first);
      dump_mc(&mc_pair.first);
    }

    if (j % 10000 == 0) {
      //usleep(39000);
    }
  
    if ((j & 0xFFFF) == 0xFFFF) {
      L_(info) << "MC analysed " << j;
      dump_mc_light(&mc_pair.first);
      L_(info) << "avg size " << data_size / mc_received;
      data_size = 0;
      mc_received = 0;
      //dump_mc(&mc_pair.first);
    }
 
    if (pending_acks == 10) {
      links.at(i)->ack_mc();
      pending_acks = 0;
    }
    j++;
  }

for (size_t i = 0; i < flib.number_of_links(); ++i) {
  L_(debug) << "disable link " << i;
  
  // disable data source
  flib.enable_mc_cnt(false);
  L_(debug) << "current mc nr 0x: " << std::hex <<  links.at(i)->mc_index();
  L_(debug) << "pending mc: "  << links.at(i)->pending_mc();
  L_(debug) << "busy: " <<  links.at(i)->channel()->isDMABusy();
 }

  L_(debug) << "Exiting";
  
    }
  catch (std::exception& e) 
    {
      L_(error) << e.what();
    }
  return 0;
}
