/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib_link_flesin.hpp"
#include <arpa/inet.h> // ntohl
#include <cassert>
#include <memory>

namespace flib {

flib_link_flesin::flib_link_flesin(size_t link_index,
                                   pda::device* dev,
                                   pda::pci_bar* bar)
    : flib_link(link_index, dev, bar) {}

//////*** Readout ***//////

void flib_link_flesin::enable_readout() {}
void flib_link_flesin::disable_readout() {}

flib_link_flesin::link_status_t flib_link_flesin::link_status() {
  uint32_t sts = m_rfgtx->get_reg(RORC_REG_GTX_LINK_STS);

  link_status_t link_status;
  link_status.channel_up = (sts & (1 << 29));
  link_status.hard_err = (sts & (1 << 28));
  link_status.soft_err = (sts & (1 << 27));
  link_status.eoe_fifo_overflow = (sts & (1 << 31));
  link_status.d_fifo_overflow = (sts & (1 << 30));
  link_status.d_fifo_max_words = (sts & 0x3FF);
  return link_status;
}

} // namespace
