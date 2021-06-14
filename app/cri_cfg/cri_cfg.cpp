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

    std::unique_ptr<cri::cri_device> cri;
    // create CRI
    if (par.cri_autodetect()) {
      cri = std::unique_ptr<cri::cri_device>(new cri::cri_device(0));
    } else {
      cri = std::unique_ptr<cri::cri_device>(new cri::cri_device(
          par.cri_addr().bus, par.cri_addr().dev, par.cri_addr().func));
    }
    std::vector<cri::cri_link*> links = cri->links();

    L_(debug) << "Configuring CRI " << cri->print_devinfo();

    // CRI global configuration
    // set even if unused
    cri->id_led(par.identify());
    cri->set_pgen_mc_size(par.mc_size());

    L_(debug) << "Tatal CRI links: " << cri->number_of_links();

    // CRI per link configuration
    for (size_t i = 0; i < cri->number_of_links(); ++i) {
      L_(debug) << "Initializing link " << i;

      // TODO: configure global datapath setting
      // reset things
      // set_mc_size_limit(par.mc_size_limit());

      if (par.link(i).source == disable) {
        links.at(i)->set_data_source(cri::cri_link::rx_disable);

      } else if (par.link(i).source == pgen) {
        links.at(i)->set_data_source(cri::cri_link::rx_pgen);

        // pgen configuration
        links.at(i)->set_pgen_rate(par.pgen_rate());
        links.at(i)->set_pgen_id(par.link(i).eq_id);

      } else if (par.link(i).source == flim) {
        links.at(i)->set_data_source(cri::cri_link::rx_user);
      }
    }

    L_(info) << "CRI " << cri->print_devinfo() << " configured";

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
