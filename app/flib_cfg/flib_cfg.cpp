/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "log.hpp"
#include "parameters.hpp"

int main(int argc, char* argv[]) {
  try {

    parameters par(argc, argv);

    std::unique_ptr<flib::flib_device_flesin> flib;
    // create FLIB
    if (par.flib_autodetect()) {
      flib = std::unique_ptr<flib::flib_device_flesin>(
          new flib::flib_device_flesin(0));
    } else {
      flib = std::unique_ptr<flib::flib_device_flesin>(
          new flib::flib_device_flesin(par.flib_addr().bus, par.flib_addr().dev,
                                       par.flib_addr().func));
    }
    std::vector<flib::flib_link_flesin*> links = flib->links();
    std::vector<std::unique_ptr<flib::flim>> flims;

    L_(debug) << "Configuring FLIB " << flib->print_devinfo();

    // FLIB global configuration
    // set even if unused
    flib->id_led(par.identify());
    flib->set_mc_time(par.mc_size());

    L_(debug) << "Tatal FLIB links: " << flib->number_of_links();

    // FLIB per link configuration
    for (size_t i = 0; i < flib->number_of_links(); ++i) {
      L_(debug) << "Initializing link " << i;

      if (par.link(i).source == disable) {
        links.at(i)->set_data_sel(flib::flib_link::rx_disable);
      } else if (par.link(i).source == pgen_near) {
        links.at(i)->set_data_sel(flib::flib_link::rx_pgen);
        // create internal FLIM
        try {
          flims.push_back(
              std::unique_ptr<flib::flim>(new flib::flim(links.at(i))));
        } catch (const std::exception& e) {
          L_(fatal) << "Link " << i << ": " << e.what();
          return EXIT_FAILURE;
        }
        flims.back()->reset_datapath();
        flims.back()->set_mc_size_limit(par.mc_size_limit());
        if (!flims.back()->get_pgen_present()) {
          L_(fatal) << "Link " << i << ": "
                    << "FLIM build does not support pgen";
          return EXIT_FAILURE;
        }
        flims.back()->set_pgen_mc_size(par.mc_size());
        flims.back()->set_pgen_rate(par.pgen_rate());
        flims.back()->set_pgen_ids(par.link(i).eq_id);
        flims.back()->set_data_source(flib::flim::pgen);
      } else if (par.link(i).source == flim || par.link(i).source == pgen_far) {
        links.at(i)->set_data_sel(flib::flib_link::rx_link);
        // create FLIM
        try {
          flims.push_back(
              std::unique_ptr<flib::flim>(new flib::flim(links.at(i))));
        } catch (const std::exception& e) {
          L_(fatal) << "Link " << i << ": " << e.what();
          return EXIT_FAILURE;
        }
        flims.back()->reset_datapath();
        flims.back()->set_mc_size_limit(par.mc_size_limit());
        if (par.link(i).source == flim) {
          flims.back()->set_data_source(flib::flim::user);
        } else { // pgen_far
          if (!flims.back()->get_pgen_present()) {
            L_(fatal) << "Link " << i << ": "
                      << "FLIM build does not support pgen";
            return EXIT_FAILURE;
          }
          flims.back()->set_pgen_mc_size(par.mc_size());
          flims.back()->set_pgen_rate(par.pgen_rate());
          flims.back()->set_pgen_ids(par.link(i).eq_id);
          flims.back()->set_data_source(flib::flim::pgen);
        }
      }
    }

    L_(info) << "FLIB " << flib->print_devinfo() << " configured";

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
