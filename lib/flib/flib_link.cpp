/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 *
 */

#include "flib_link.hpp"
#include <cassert>

#define DMA_TRANSFER_SIZE 128

namespace flib {
flib_link::flib_link(size_t link_index, pda::device* dev, pda::pci_bar* bar)
    : m_link_index(link_index), m_parent_device(dev), m_bar(bar) {
  m_base_addr = (m_link_index + 1) * RORC_CHANNEL_OFFSET;
  // register file access
  m_rfpkt =
      std::unique_ptr<register_file>(new register_file_bar(bar, m_base_addr));
  m_rfgtx = std::unique_ptr<register_file>(
      new register_file_bar(bar, (m_base_addr + (1 << RORC_DMA_CMP_SEL))));
}

flib_link::~flib_link() { deinit_dma(); }

void flib_link::init_dma(void* data_buffer,
                         size_t data_buffer_log_size,
                         void* desc_buffer,
                         size_t desc_buffer_log_size) {

  m_dma_channel = std::unique_ptr<dma_channel>(
      new dma_channel(this, data_buffer, data_buffer_log_size, desc_buffer,
                      desc_buffer_log_size, DMA_TRANSFER_SIZE));
}

void flib_link::init_dma(size_t data_buffer_log_size,
                         size_t desc_buffer_log_size) {

  m_dma_channel = std::unique_ptr<dma_channel>(new dma_channel(
      this, data_buffer_log_size, desc_buffer_log_size, DMA_TRANSFER_SIZE));
}

void flib_link::deinit_dma() { m_dma_channel = nullptr; }

dma_channel* flib_link::channel() const {
  if (m_dma_channel) {
    return m_dma_channel.get();
  }
  throw FlibException("DMA channel not initialized");
}

//////*** DPB Emualtion ***//////

void flib_link::reset_datapath() {
  // disable readout if still enabled
  disable_readout();
  // datapath reset, will also cause hw defaults for
  // - pending mc  = 0
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, true);
  // TODO this may be needed in case of errors
  // if (m_dma_channel) {
  //  m_dma_channel->reset_fifo(true);
  //}
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, false);
}

void flib_link::reset_link() {
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 3, true);
  usleep(1000);
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 3, false);
}

void flib_link::set_data_sel(data_sel_t rx_sel) {
  uint32_t dp_cfg = m_rfgtx->get_reg(RORC_REG_GTX_DATAPATH_CFG);
  switch (rx_sel) {
  case rx_disable:
    m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg & ~3));
    break;
  case rx_link:
    m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | (1 << 1)) & ~1));
    break;
  case rx_pgen:
    m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg | 3));
    break;
  case rx_emu:
    m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | 1)) & ~(1 << 1));
    break;
  }
}

flib_link::data_sel_t flib_link::data_sel() {
  uint32_t dp_cfg = m_rfgtx->get_reg(RORC_REG_GTX_DATAPATH_CFG);
  return static_cast<data_sel_t>(dp_cfg & 0x3);
}

std::ostream& operator<<(std::ostream& os, flib::flib_link::data_sel_t sel) {
  switch (sel) {
  case flib::flib_link::rx_disable:
    os << "disable";
    break;
  case flib::flib_link::rx_emu:
    os << "    emu";
    break;
  case flib::flib_link::rx_link:
    os << "   link";
    break;
  case flib::flib_link::rx_pgen:
    os << "   pgen";
    break;
  default:
    os.setstate(std::ios_base::failbit);
  }
  return os;
}

} // namespace flib
