#include <csignal>

#include <zmq.hpp>
#include <control/libserver/ControlServer.hpp>

#include "global.hpp"
#include "flib_server.hpp"

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

  // create ZMQ context
  zmq::context_t zmq_context(1);

  // create FLIB
  flib::flib_device flib(0);
  std::vector<flib::flib_link*> links = flib.get_links();

  // configure FLIB link
  links.at(0)->set_data_rx_sel(flib::flib_link::pgen);

  // create device control server, initialize and start server thread
  flib_server flibserver(zmq_context, "CbmNet::Driver0", *links.at(0));
  flibserver.Bind();
  flibserver.Start();
  
  // create control server and start
  CbmNet::ControlServer cntlserv(zmq_context, "CbmNet::Driver0");
  //cntlserv.SetDebugFlags(CbmNet::ControlServer::kDbgDumpRpc);
  cntlserv.Bind();
  cntlserv.ConnectDriver();
  cntlserv.Start();

  // main loop
  while(s_interrupted==0) {
    ::sleep(1);
  }
  out.debug() << "Exiting";

  return 0;
}
