// Copyright 2015 Dirk Hutter

#include <csignal>
#include "log.hpp"

#include "parameters.hpp"
#include "shm_device_server.hpp"

namespace
{
  volatile std::sig_atomic_t signal_status = 0;
}

static void signal_handler(int sig) {
  signal_status = sig;
}


int main(int argc, char* argv[])
{

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
  
  try {
    
    parameters par(argc, argv);
    
    std::unique_ptr<flib_device> flib =
      std::unique_ptr<flib_device>(new flib_device(0));
    std::vector<flib_link*> links = flib->links();
    
    // FLIB global configuration
    flib->set_mc_time(par.mc_size());
    L_(debug) << "MC size is: " 
              << (flib->rf()->reg(RORC_REG_MC_CNT_CFG) & 0x7FFFFFFF);
    
    // FLIB per link configuration
    for (size_t i = 0; i < flib->number_of_links(); ++i) {
      L_(debug) << "Initializing link " << i;   
      link_config_t link_config = par.link_config(i);
      links.at(i)->set_hdr_config(&link_config.hdr_config);
      links.at(i)->set_data_sel(link_config.rx_sel);
      links.at(i)->enable_cbmnet_packer_debug_mode(par.debug_mode());
    }

    // create server
    shm_device_server server(flib.get(), 27, 23, &signal_status);    
    server.run();
    
    
  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
  }
  
  return 0;
}




