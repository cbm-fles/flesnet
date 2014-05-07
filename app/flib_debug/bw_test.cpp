// Simpel example application initializing a FLIB for date transfer

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <chrono>

#include <flib.h>
#include "global.hpp"

#include "mc_functions.h"

// Configuration
uint32_t mc_time = 1000;
size_t num_links = 4;
size_t ack_delay = 100;
size_t max_mc = 1000000;

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
  
  if ( argc != 4) {
    out.error() << "provide all parameter";
    return -1;
  }

  mc_time = atoi(argv[1]);
  out.debug() << "mc_time set to " << mc_time;

  max_mc = atoi(argv[2]);
  out.debug() << "max_mc set to " << max_mc;

  num_links = atoi(argv[3]);
  out.debug() << "mum_links set to " << num_links;

  // create FLIB
  try 
  {
  flib::flib_device flib(0);

  std::vector<flib::flib_link*> links = flib.get_links();

  out.info() << flib.print_build_info();

  out.debug() << "Set MC time to "<< mc_time;
  flib.set_mc_time(mc_time);

  std::vector<uint64_t*> eb;
  std::vector<MicrosliceDescriptor*> rb;

  size_t flib_links = flib.get_num_links();
  num_links = (num_links < flib_links) ? num_links : flib_links;
  out.debug() << "using " << num_links << " flib links";

  for (size_t i = 0; i < num_links; ++i) {
    out.debug() << "initializing link " << i;

    // initialize a DMA buffer
    links.at(i)->init_dma(flib::create_only, 29, 27);
    out.info() << "Event Buffer: " << links.at(i)->get_ebuf_info();
    out.info() << "Desciptor Buffer" << links.at(i)->get_dbuf_info();
    // get raw pointers for debugging
    eb.push_back((uint64_t *)links.at(i)->ebuf()->getMem());
    rb.push_back((MicrosliceDescriptor *)links.at(i)->rbuf()->getMem());
    
    // set start index
    links.at(i)->set_start_idx(1);
    // trottle pgen
    //links.at(i)->set_mc_pgen_throttle(0x7FFF);
    links.at(i)->set_mc_pgen_throttle(0);
    
    // set the aproriate header config
    flib::hdr_config config = {};
    config.eq_id = 0xE003;
    config.sys_id = 0xBC;
    config.sys_ver = 0xFD;
    links.at(i)->set_hdr_config(&config);
    
    // enable data source
    bool use_sp = false; 
    if (use_sp == true) {
      links.at(i)->set_data_rx_sel(flib::flib_link::link);
      links.at(i)->prepare_dlm(0x8, true); // enable DAQ
      links.at(i)->send_dlm();
    } 
    else {
      links.at(i)->set_data_rx_sel(flib::flib_link::pgen);
    }
        
    links.at(i)->enable_cbmnet_packer(true);
    
    out.debug() << "current mc nr: " <<  links.at(i)->get_mc_index();
    out.debug() << "link " << i << "initialized";
  }

  /////////// THE MAIN LOOP ///////////
  // service only one link for the moment

  std::vector<size_t> mc_num (num_links, 0);
  std::vector<size_t> pending_acks (num_links, 0);
  std::vector<size_t> rx_data (num_links, 0);
  std::vector<size_t> wait (num_links, 0);

  auto time_begin = std::chrono::high_resolution_clock::now();
  flib.enable_mc_cnt(true);

  while(s_interrupted==0) {

    for (size_t i = 0; i < num_links; ++i) { // serve link round robin
      
      std::pair<flib::mc_desc, bool> mc_pair;
      //while ((mc_pair = links.at(i)->get_mc()).second == false && s_interrupted==0) {
      //  wait.at(i)++;
      //  //usleep(10);
      //  //links.at(i)->ack_mc();
      //  //pending_acks.at(i) = 0;
      //}
      if ((mc_pair = links.at(i)->get_mc()).second == false) {
        wait.at(i)++;
        continue; // jump to nex link if nothin is available
      }

      pending_acks.at(i)++;
      
      if (s_interrupted != 0) {
        break;
      }
      
      rx_data.at(i) = ((MicrosliceDescriptor*)mc_pair.first.rbaddr)->offset;
      
      if (mc_num.at(i) == 0) {
        out.info() << "First MC seen.";
        dump_mc_light(&mc_pair.first);
        //std::cout << "RB:" << std::endl;
        //dump_raw((uint64_t *)(rb.at(i)+mc_num.at(i)),16);
        //std::cout << "EB:" << std::endl;
        //dump_raw((uint64_t *)(eb.at(i)), 64);
      }
      
      if (mc_num.at(i) != 0 && mc_num.at(i) < 1) {
        out.info() << "MC analysed " << mc_num.at(i);
        out.info() << "rx_data " << rx_data.at(i);
        out.info() << "size per MC " << std::setprecision(10) << (double)rx_data.at(i) / (double)mc_num.at(i);
        //dump_mc_light(&mc_pair.first);
        //dump_mc(&mc_pair.first);
      }
      
      if ((mc_num.at(i) & 0xFFFFF) == 0xFFFFF) {
        out.info() << "MC analysed " << mc_num.at(i);
        out.info() << "rx_data (offset) " << rx_data.at(i);
        out.info() << "size per MC " << std::setprecision(10) << (double)rx_data.at(i)  / (double)mc_num.at(i);
        
        //dump_mc_light(&mc_pair.first);
        //dump_mc(&mc_pair.first);
      }
      
      if (pending_acks.at(i) == ack_delay) {
        links.at(i)->ack_mc();
        pending_acks.at(i) = 0;
      }
      if (mc_num.at(i) == max_mc) {
        s_interrupted = 1;
      }

      mc_num.at(i)++;
    }
  }
  
  auto time_end = std::chrono::high_resolution_clock::now();

  double runtime = std::chrono::duration_cast<std::chrono::microseconds>
    (time_end - time_begin).count();
  
  out.info() << "runtime " << runtime/1000000 << " s";

  size_t acc_data = 0;

for (size_t i = 0; i < num_links; ++i) {
  out.debug() << "disable link " << i;
  
  // disable data source
  flib.enable_mc_cnt(false);
  out.debug() << "current mc nr 0x: " << std::hex <<  links.at(i)->get_mc_index();
  out.debug() << "pending mc: "  << links.at(i)->get_pending_mc();
  out.debug() << "busy: " <<  links.at(i)->get_ch()->getDMABusy();

  out.info() << "total data " << rx_data.at(i) << " Bytes";
  out.info() << "bandwidth " << (double)rx_data.at(i) / runtime << " MB/s";
  out.info() << "SW waiting/mc_num " << (double)wait.at(i) / (double)mc_num.at(i);

  acc_data += rx_data.at(i);
 }

 out.info() << "accumulated Bandwidth " << (double)acc_data / runtime << " MB/s";
 out.debug() << "Exiting";
  
    }
  catch (std::exception& e) 
    {
      out.error() << e.what();
    }
  return 0;
}
