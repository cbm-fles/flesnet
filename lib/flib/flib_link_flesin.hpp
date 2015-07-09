/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include <flib_link.hpp>

namespace flib {

class flib_link_flesin : public flib_link {

public:
  flib_link_flesin(size_t link_index, pda::device* dev, pda::pci_bar* bar);

  void set_pgen_rate(float val);
};
} // namespace
