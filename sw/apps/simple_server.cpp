// Simpel example application initializing a FLIB for date and control transfer

#include <iostream>
#include <csignal>
#include <boost/thread.hpp>

#include "../../../usbdaq-test/cbmnet/zmq.hpp"
#include "../../../usbdaq-test/cbmnet/control/libserver/ControlServer.hpp"

#include "global.hpp"
#include "flib_control_server.hpp"

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

  zmq::context_t zmq_context(1);

  // create FLIB
  flib::flib_device flib(0);
  std::vector<flib::flib_link*> links = flib.get_links();

  // create a device control server, initialize and start server thread
  flib_control_server flibserver(zmq_context, "CbmNet::Driver0", *links.at(0));
  flibserver.Bind();
  flibserver.Start();
  
  // create control server and start
  CbmNet::ControlServer cntlserv(zmq_context, "CbmNet::Driver0");
  cntlserv.SetDebugFlags(CbmNet::ControlServer::kDbgDumpRpc);
  cntlserv.Bind();
  cntlserv.ConnectDriver();
  cntlserv.Start();

  // initialize FLIB links: ////
  // initialize a DMA buffer
  links.at(0)->init_dma(flib::create_only, 22, 20);
  out.info() << "Event Buffer: " << links.at(0)->get_ebuf_info();
  out.info() << "Desciptor Buffer" << links.at(0)->get_dbuf_info();

  // set the aproriate header config
  flib::hdr_config config = {};
  config.eq_id = 0xE003;
  config.sys_id = 0xBC;
  config.sys_ver = 0xFD;
  links.at(0)->set_hdr_config(&config);

  // reset pending mc from potential previous run
  links.at(0)->rst_pending_mc();

  // set data source and enable
  links.at(0)->set_data_rx_sel(flib::flib_link::pgen);
  links.at(0)->enable_cbmnet_packer(true);
  flib.enable_mc_cnt(true);

  // main loop

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

    if (j == 0) {
      out.info() << "First MC seen.";
    }
    if ((j & 0xFFFFF) == 0xFFFFF) {
      out.info() << "MC analysed" << j;
    }
    if (pending_acks == 100) {
      links.at(0)->ack_mc();
      pending_acks = 0;
    }
    j++;
  }
  
  // disable data source
  flib.enable_mc_cnt(false);
  out.debug() << "pending mc :" <<  links.at(0)->get_pending_mc();
  links.at(0)->rst_pending_mc();

  out.debug() << "Exiting";
  
  return 0;
}
