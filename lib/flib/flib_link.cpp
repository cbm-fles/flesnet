/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 *
 */

#include <cassert>
#include <memory>

#include <flib_link.hpp>

#define DMA_TRANSFER_SIZE 128

namespace flib {
flib_link::flib_link(size_t link_index, pda::device* dev, pda::pci_bar* bar)
    : m_link_index(link_index), m_parent_device(dev) {
  m_base_addr = (m_link_index + 1) * RORC_CHANNEL_OFFSET;
  // register file access
  m_rfpkt =
      std::unique_ptr<register_file>(new register_file_bar(bar, m_base_addr));
  m_rfgtx = std::unique_ptr<register_file>(
      new register_file_bar(bar, (m_base_addr + (1 << RORC_DMA_CMP_SEL))));
  m_rfglobal = std::unique_ptr<register_file>(new register_file_bar(bar, 0));
}

flib_link::~flib_link() { deinit_dma(); }

void flib_link::init_dma(void* data_buffer,
                         size_t data_buffer_log_size,
                         void* desc_buffer,
                         size_t desc_buffer_log_size) {

  m_dma_channel =
      std::unique_ptr<dma_channel>(new dma_channel(this,
                                                   data_buffer,
                                                   data_buffer_log_size,
                                                   desc_buffer,
                                                   desc_buffer_log_size,
                                                   DMA_TRANSFER_SIZE));
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
  } else {
    throw FlibException("DMA channel not initialized");
  }
}

//////*** DPB Emualtion ***//////

void flib_link::init_datapath() { set_start_idx(1); }

void flib_link::reset_datapath() {
  // disable packer if still enabled
  enable_cbmnet_packer(false);
  // datapath reset, will also cause hw defaults for
  // - pending mc  = 0
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, true);
  // TODO this may be needed in case of errors
  // if (m_dma_channel) {
  //  m_dma_channel->reset_fifo(true);
  //}
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, false);
}

void flib_link::rst_cnet_link() {
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 3, true);
  usleep(1000);
  m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 3, false);
}

void flib_link::set_start_idx(uint64_t index) {
  // set reset value
  // TODO replace with _rfgtx->set_mem()
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG_IDX_L,
                   static_cast<uint32_t>(index & 0xffffffff));
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG_IDX_H,
                   static_cast<uint32_t>(index >> 32));
  // reste mc counter
  // TODO implenet edge detection and 'pulse only' in HW
  uint32_t mc_gen_cfg = m_rfgtx->get_reg(RORC_REG_GTX_MC_GEN_CFG);
  // TODO replace with _rfgtx->set_bit()
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | 1));
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1)));
}

void flib_link::rst_pending_mc() {
  // Is also resetted with datapath reset
  // TODO implenet edge detection and 'pulse only' in HW
  uint32_t mc_gen_cfg = m_rfgtx->get_reg(RORC_REG_GTX_MC_GEN_CFG);
  // TODO replace with _rfgtx->set_bit()
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | (1 << 1)));
  m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1 << 1)));
}

void flib_link::enable_cbmnet_packer(bool enable) {
  m_rfgtx->set_bit(RORC_REG_GTX_MC_GEN_CFG, 2, enable);
}

void flib_link::enable_cbmnet_packer_debug_mode(bool enable) {
  m_rfgtx->set_bit(RORC_REG_GTX_MC_GEN_CFG, 3, enable);
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

void flib_link::set_hdr_config(const hdr_config_t* config) {
  m_rfgtx->set_mem(RORC_REG_GTX_MC_GEN_CFG_HDR,
                   static_cast<const void*>(config),
                   sizeof(hdr_config_t) >> 2);
}

uint64_t flib_link::pending_mc() {
  // TODO replace with _rfgtx->get_mem()
  uint64_t pend_mc = m_rfgtx->get_reg(RORC_REG_GTX_PENDING_MC_L);
  pend_mc = pend_mc |
            (static_cast<uint64_t>(m_rfgtx->get_reg(RORC_REG_GTX_PENDING_MC_H))
             << 32);
  return pend_mc;
}

// TODO this has to become desc_offset() in libflib2
uint64_t flib_link::mc_index() {
  uint64_t mc_index = m_rfgtx->get_reg(RORC_REG_GTX_MC_INDEX_L);
  mc_index =
      mc_index |
      (static_cast<uint64_t>(m_rfgtx->get_reg(RORC_REG_GTX_MC_INDEX_H)) << 32);
  return mc_index;
}

} // namespace
