/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <cassert>
#include <memory>

#include <flib_link_flesin.hpp>

namespace flib {

flib_link_flesin::flib_link_flesin(size_t link_index,
                                   pda::device* dev,
                                   pda::pci_bar* bar)
    : flib_link(link_index, dev, bar) {
  m_rflink = std::unique_ptr<register_file>(new register_file_bar(
      bar, (m_base_addr + (1 << RORC_DMA_CMP_SEL) + (1 << 5))));
}

//////*** Readout ***//////

void flib_link_flesin::enable_readout(bool enable) { (void)enable; }

void flib_link_flesin::set_pgen_rate(float val) {
  assert(val >= 0);
  assert(val <= 1);
  uint16_t reg_val =
      static_cast<uint16_t>(static_cast<float>(UINT16_MAX) * (1.0 - val));
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG,
                   static_cast<uint32_t>(reg_val) << 16,
                   0xFFFF0000);
}

void flib_link_flesin::set_testreg(uint32_t data) {
  m_rflink->set_reg(RORC_REG_LINK_TEST, data);
}

uint32_t flib_link_flesin::get_testreg() {
  return m_rflink->get_reg(RORC_REG_LINK_TEST);
}

} // namespace
