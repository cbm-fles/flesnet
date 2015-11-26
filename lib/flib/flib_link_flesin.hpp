/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include "boost/date_time/posix_time/posix_time.hpp"

#include <flib_link.hpp>

namespace flib {

class flib_link_flesin : public flib_link {

public:
  flib_link_flesin(size_t link_index, pda::device* dev, pda::pci_bar* bar);

  void enable_readout(bool enable) override;
  void set_pgen_rate(float val);

private:
};
} // namespace
