#include <csignal>

#include <zmq.hpp>
#include <boost/lexical_cast.hpp>
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

  out.setVerbosity(einhard::TRACE);

  // create ZMQ context
  zmq::context_t zmq_context(1);

  // create FLIB
  flib::flib_device flib(0);
  std::vector<flib::flib_link*> links = flib.get_links();

  // FLIB global configuration
  flib.set_mc_time(1000);

  // FLIB per link configuration
  std::vector<std::unique_ptr<flib_server>> flibserver;
  std::vector<std::unique_ptr<CbmNet::ControlServer>> ctrlserver;

  for (size_t i = 0; i < flib.get_num_links(); ++i) {
    out.debug() << "initializing link " << i;

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

    // create device control server, initialize and start server thread
    flibserver.push_back(std::unique_ptr<flib_server>(
        new flib_server(zmq_context, "CbmNet::Driver" + 
                        boost::lexical_cast<std::string>(i), 
                        flib, *links.at(i))));
    flibserver.at(i)->Bind();
    flibserver.at(i)->Start();

    // create control server and start
    ctrlserver.push_back(std::unique_ptr<CbmNet::ControlServer>(
         new CbmNet::ControlServer(zmq_context, "CbmNet::Driver" 
                                   + boost::lexical_cast<std::string>(i))));
    //ctrlserver.at(i)->SetDebugFlags(CbmNet::ControlServer::kDbgDumpRpc);    
    ctrlserver.at(i)->Bind("tcp://*:" + boost::lexical_cast<std::string>(9750 + i));
    ctrlserver.at(i)->ConnectDriver();
    ctrlserver.at(i)->Start();

  }

  // main loop
  while(s_interrupted==0) {
    ::sleep(1);
  }
  out.debug() << "Exiting";

  return 0;
}
