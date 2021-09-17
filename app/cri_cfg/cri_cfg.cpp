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
    if (par.dev_autodetect()) {
      cri = std::unique_ptr<cri::cri_device>(new cri::cri_device(0));
    } else {
      cri = std::unique_ptr<cri::cri_device>(new cri::cri_device(
          par.dev_addr().bus, par.dev_addr().dev, par.dev_addr().func));
    }
    std::vector<cri::cri_channel*> channels = cri->channels();

    L_(debug) << "Configuring CRI " << cri->print_devinfo();

    // CRI global configuration
    // set even if unused
    cri->id_led(par.identify());
    cri->set_pgen_mc_size(par.mc_size());

    L_(debug) << "Tatal CRI channels: " << cri->number_of_channels();

    // CRI per channel configuration
    for (size_t i = 0; i < cri->number_of_channels(); ++i) {
      L_(debug) << "Initializing channel " << i;

      channels.at(i)->set_mc_size_limit(par.mc_size_limit());

      if (par.channel(i).source == disable) {
        channels.at(i)->set_data_source(cri::cri_channel::rx_disable);

      } else if (par.channel(i).source == pgen) {
        channels.at(i)->set_data_source(cri::cri_channel::rx_pgen);

        // pgen configuration
        channels.at(i)->set_pgen_rate(par.pgen_rate());
        channels.at(i)->set_pgen_id(par.channel(i).eq_id);

      } else if (par.channel(i).source == flim) {
        channels.at(i)->set_data_source(cri::cri_channel::rx_user);
      }
    }

    L_(info) << "CRI " << cri->print_devinfo() << " configured";

  } catch (std::exception const& e) {
    L_(fatal) << "exception: " << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
