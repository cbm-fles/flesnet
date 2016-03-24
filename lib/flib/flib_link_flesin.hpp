/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "flib_link.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace flib {

class flib_link_flesin : public flib_link {

public:
  flib_link_flesin(size_t link_index, pda::device* dev, pda::pci_bar* bar);

  void enable_readout() override;
  void disable_readout() override;

private:
};
} // namespace
