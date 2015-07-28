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

  void enable_readout(bool enable) override;
  void set_pgen_rate(float val);

  void set_testreg(uint32_t data);

  uint32_t get_testreg();

private:
  std::unique_ptr<register_file> m_rflink;
};
} // namespace
