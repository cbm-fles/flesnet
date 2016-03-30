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

  typedef struct {
    bool channel_up;
    bool hard_err;
    bool soft_err;
    bool eoe_fifo_overflow;
    bool d_fifo_overflow;
    uint16_t d_fifo_max_words;
  } link_status_t;

  link_status_t link_status();

private:
};
} // namespace
