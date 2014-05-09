// FLIB Bandwidth micro benchmark

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
  size_t mc_size = mc_time * 8;
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
 
  std::vector<size_t> mc_num (num_links, 0);
  std::vector<size_t> pending_acks (num_links, 0);
  std::vector<size_t> pcie_data (num_links, 0);
  std::vector<size_t> payload_data (num_links, 0);
  std::vector<size_t> wait (num_links, 0);
  std::vector<size_t> sum_mc (num_links, 0);

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
      
      // byte count: pcie_data = mc_size incl. padding to max_payload
      pcie_data.at(i) = ((MicrosliceDescriptor*)mc_pair.first.rbaddr)->offset;
      payload_data.at(i) += mc_pair.first.size;
      
      if (mc_num.at(i) == 0) {
        out.info() << "First MC seen.";
        dump_mc_light(&mc_pair.first);
      }
      
      if (mc_num.at(i) != 0 && mc_num.at(i) < 2) {
        out.info() << "MC analysed " << mc_num.at(i);
        out.info() << "pcie_data (padded size) " << pcie_data.at(i) << " Bytes";
        out.info() << "payload_data (payload size) " << payload_data.at(i) << " Bytes";
        out.info() << "size per MC " << std::setprecision(10) << (double)pcie_data.at(i) / (double)mc_num.at(i);
      }
      
      if ((mc_num.at(i) & 0xFFFFF) == 0xFFFFF) {
        out.info() << "MC analysed " << mc_num.at(i);
        out.info() << "pcie_data (padded size) " << pcie_data.at(i) << " Bytes";
        out.info() << "payload_data (payload size) " << payload_data.at(i) << " Bytes";
        out.info() << "size per MC " << std::setprecision(10) << (double)pcie_data.at(i)  / (double)mc_num.at(i);
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

  size_t acc_pcie_data = 0;
  size_t acc_payload_data = 0;
  size_t acc_sum_mc = 0;
  
for (size_t i = 0; i < num_links; ++i) {
  out.debug() << "disable link " << i;
  
  // disable data source
  flib.enable_mc_cnt(false);
  
  out.debug() << "current mc nr 0x: " << std::hex <<  links.at(i)->get_mc_index();
  out.debug() << "pending mc: "  << links.at(i)->get_pending_mc();
  out.debug() << "busy: " <<  links.at(i)->get_ch()->getDMABusy();

  out.info() << "total pcie data " << pcie_data.at(i) << " Bytes";
  out.info() << "total payload data " << payload_data.at(i) << " Bytes";
  out.info() << "pcie bandwidth " << (double)pcie_data.at(i) / runtime << " MB/s";
  out.info() << "payload bandwidth " << (double)payload_data.at(i) / runtime << " MB/s";
  out.info() << "SW waiting/mc_num " << (double)wait.at(i) / (double)mc_num.at(i);

  sum_mc.at(i) = links.at(i)->get_mc_index();

  acc_sum_mc += sum_mc.at(i);
  acc_pcie_data += pcie_data.at(i);
  acc_payload_data += payload_data.at(i);

 }

 double acc_pcie_bw = (double)acc_pcie_data / runtime;
 double acc_payload_bw = (double)acc_payload_data / runtime;
 double acc_desc_bw = (double)(acc_sum_mc * sizeof(MicrosliceDescriptor)) / runtime;


 out.info() << "Total MCs submitted " << acc_sum_mc << " (expected " << max_mc * num_links << " )";
 out.info() << "accumulated pci bandwidth " << (double)acc_pcie_data / runtime << " MB/s";
 out.info() << "accumulated payload bandwidth " << (double)acc_payload_data / runtime << " MB/s";
 out.info() << "accumulated descriptor bandwidth " << acc_desc_bw << " MB/s";
 std::cout << "KEY num_links mc_time [8ns] mc_size [Bytes] num_mc pcie_bw [MB/s] payload_bw [MB/s] dec_bw [MB/s] pcie_dec_bw [MB/s]" << std::endl;
 std::cout << "SUMMARY "
           << num_links << " " << mc_time << " " << mc_size  << " " << max_mc << " " 
           << acc_pcie_bw << " " << acc_payload_bw << " " 
           << acc_desc_bw << " " <<  acc_desc_bw + acc_pcie_bw << std::endl;

 out.debug() << "Exiting";
  
    }
  catch (std::exception& e) 
    {
      out.error() << e.what();
    }
  return 0;
}
