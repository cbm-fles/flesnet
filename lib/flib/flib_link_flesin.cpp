/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <cassert>
#include <memory>
#include <arpa/inet.h> // ntohl

#include <flib_link_flesin.hpp>

namespace flib {

flib_link_flesin::flib_link_flesin(size_t link_index,
                                   pda::device* dev,
                                   pda::pci_bar* bar)
    : flib_link(link_index, dev, bar) {}

//////*** Readout ***//////

void flib_link_flesin::enable_readout() {}
void flib_link_flesin::disable_readout() {}

} // namespace
