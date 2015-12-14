/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <csignal>

#include <boost/lexical_cast.hpp>

#include <log.hpp>
#include "parameters.hpp"

int s_interrupted = 0;
static void s_signal_handler(int signal_value) { s_interrupted = 1; }

static void s_catch_signals(void) {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGABRT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);
}

int main(int argc, char* argv[]) {
  s_catch_signals();
  int ret = 0;

  parameters par(argc, argv);

  // create FLIB
  flib::flib_device_flesin flib(0);
  std::vector<flib::flib_link_flesin*> links = flib.links();
  std::vector<std::unique_ptr<flib::flim>> flims;

  // FLIB global configuration
  // set even if unused
  flib.set_mc_time(par.mc_size());

  L_(debug) << "Tatal FLIB links: " << flib.number_of_links();

  // FLIB per link configuration
  for (size_t i = 0; i < flib.number_of_links(); ++i) {
    L_(debug) << "Initializing link " << i;

    if (par.link(i).source == disable) {
      links.at(i)->set_data_sel(flib::flib_link::rx_disable);
    } else if (par.link(i).source == pgen_near) {
      links.at(i)->set_pgen_rate(par.pgen_rate());
      flib::flib_link::hdr_config_t hdr_cfg = {par.link(i).eq_id, 0, 0};
      links.at(i)->set_hdr_config(&hdr_cfg);
      links.at(i)->set_data_sel(flib::flib_link::rx_pgen);
    } else if (par.link(i).source == flim || par.link(i).source == pgen_far) {
      links.at(i)->set_data_sel(flib::flib_link::rx_link);
      // create FLIM
      try {
        flims.push_back(
            std::unique_ptr<flib::flim>(new flib::flim(links.at(i))));
      } catch (const std::exception& e) {
        L_(error) << e.what();
        exit(EXIT_FAILURE);
      }
      flims.at(i)->reset();
      flims.at(i)->set_start_idx(0);
      if (par.link(i).source == flim) {
        flims.at(i)->set_data_source(flib::flim::user);
      } else { // pgen_far
        if (!flims.at(i)->get_pgen_present()) {
          L_(error) << "FLIM build does not support pgen";
          exit(EXIT_FAILURE);
        }
        flims.at(i)->set_pgen_mc_size(par.mc_size());
        flims.at(i)->set_pgen_rate(par.pgen_rate());
        flims.at(i)->set_pgen_ids(par.link(i).eq_id);
        flims.at(i)->set_data_source(flib::flim::pgen);
      }
    }
  }

  L_(debug) << "Exiting";

  return ret;
}
