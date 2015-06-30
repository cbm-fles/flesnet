#include <csignal>

#include <zmq.hpp>
#include <boost/lexical_cast.hpp>

#include <log.hpp>
#include "parameters.hpp"

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
  flib::flib_device_flesin flib(0);
  std::vector<flib::flib_link_flesin*> links = flib.links();

  // FLIB global configuration
  flib.set_mc_time(par.mc_size());
  L_(debug) << "MC size is: " 
              << (flib.rf()->reg(RORC_REG_MC_CNT_CFG) & 0x7FFFFFFF);

  // FLIB per link configuration
  for (size_t i = 0; i < flib.number_of_links(); ++i) {
    L_(debug) << "Initializing link " << i;

    struct link_config link_config = par.link_config(i);
    link_config.hdr_config.eq_id = static_cast<uint16_t>(0xE000 + i);
    links.at(i)->set_hdr_config(&link_config.hdr_config);
    links.at(i)->set_data_sel(link_config.rx_sel);
    links.at(i)->enable_cbmnet_packer_debug_mode(par.debug_mode());
  }

  L_(debug) << "Exiting";

  return 0;
}
