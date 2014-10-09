#include <csignal>

#include <zmq.hpp>
#include <boost/lexical_cast.hpp>
#ifdef CNETCNTLSERVER
#include <control/libserver/ControlServer.hpp>
#endif

#include <log.hpp>
#include "parameters.hpp"
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


int main(int argc, char* argv[])
{
  s_catch_signals();

  parameters par(argc, argv);

  // create ZMQ context
  zmq::context_t zmq_context(1);

  // create FLIB
  flib::flib_device flib(0);
  std::vector<flib::flib_link*> links = flib.links();

  // FLIB global configuration
  flib.set_mc_time(par.mc_size());
  L_(debug) << "MC size is: " 
              << (flib.rf()->reg(RORC_REG_MC_CNT_CFG) & 0x7FFFFFFF);

  // FLIB per link configuration
#ifdef CNETCNTLSERVER
  std::vector<std::unique_ptr<flib_server>> flibserver;
  std::vector<std::unique_ptr<CbmNet::ControlServer>> ctrlserver;
#else
  L_(info) << "Compiled without controls support. Configuring FLIB and exit.";
#endif

  for (size_t i = 0; i < flib.number_of_links(); ++i) {
    L_(debug) << "Initializing link " << i;

    struct link_config link_config = par.link_config(i);
    link_config.hdr_config.eq_id = static_cast<uint16_t>(0xE000 + i);
    links.at(i)->set_hdr_config(&link_config.hdr_config);
    links.at(i)->set_data_sel(link_config.rx_sel);

#ifdef CNETCNTLSERVER
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
#endif
  }

#ifdef CNETCNTLSERVER
  L_(info) << "FLIB configured successfully. Waiting to forward control commands. Send ctrl+c to exit.";
  // main loop
  while(s_interrupted==0) {
    ::sleep(1);
  }
#endif
  L_(debug) << "Exiting";

  return 0;
}
