/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include "flib_link_cnet.hpp"
#include <cassert>
#include <memory>

namespace flib {

flib_link_cnet::flib_link_cnet(size_t link_index,
                               pda::device* dev,
                               pda::pci_bar* bar)
    : flib_link(link_index, dev, bar) {}

//////*** Readout ***//////
void flib_link_cnet::set_start_idx(uint64_t index) {
  // set reset value
  // TODO replace with _rfgtx->set_mem()
  m_rfgtx->set_reg(CNET_REG_GTX_MC_GEN_CFG_IDX_L,
                   static_cast<uint32_t>(index & 0xffffffff));
  m_rfgtx->set_reg(CNET_REG_GTX_MC_GEN_CFG_IDX_H,
                   static_cast<uint32_t>(index >> 32));
  // reste mc counter
  // TODO implenet edge detection and 'pulse only' in HW
  uint32_t mc_gen_cfg = m_rfgtx->get_reg(CNET_REG_GTX_MC_GEN_CFG);
  // TODO replace with _rfgtx->set_bit()
  m_rfgtx->set_reg(CNET_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | 1));
  m_rfgtx->set_reg(CNET_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1)));
}

void flib_link_cnet::rst_pending_mc() {
  // Is also resetted with datapath reset
  // TODO implenet edge detection and 'pulse only' in HW
  uint32_t mc_gen_cfg = m_rfgtx->get_reg(CNET_REG_GTX_MC_GEN_CFG);
  // TODO replace with _rfgtx->set_bit()
  m_rfgtx->set_reg(CNET_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | (1 << 1)));
  m_rfgtx->set_reg(CNET_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1 << 1)));
}

void flib_link_cnet::set_hdr_config(const hdr_config_t* config) {
  m_rfgtx->set_mem(CNET_REG_GTX_MC_GEN_CFG_HDR,
                   static_cast<const void*>(config), sizeof(hdr_config_t) >> 2);
}

uint64_t flib_link_cnet::pending_mc() {
  // TODO replace with _rfgtx->get_mem()
  uint64_t pend_mc = m_rfgtx->get_reg(CNET_REG_GTX_PENDING_MC_L);
  pend_mc = pend_mc |
            (static_cast<uint64_t>(m_rfgtx->get_reg(CNET_REG_GTX_PENDING_MC_H))
             << 32);
  return pend_mc;
}

uint64_t flib_link_cnet::mc_index() {
  uint64_t mc_index = m_rfgtx->get_reg(CNET_REG_GTX_MC_INDEX_L);
  mc_index =
      mc_index |
      (static_cast<uint64_t>(m_rfgtx->get_reg(CNET_REG_GTX_MC_INDEX_H)) << 32);
  return mc_index;
}

void flib_link_cnet::enable_cbmnet_packer(bool enable) {
  m_rfgtx->set_bit(CNET_REG_GTX_MC_GEN_CFG, 2, enable);
}

void flib_link_cnet::enable_cbmnet_packer_debug_mode(bool enable) {
  m_rfgtx->set_bit(CNET_REG_GTX_MC_GEN_CFG, 3, enable);
}

void flib_link_cnet::enable_readout() {
  set_start_idx(0);
  enable_cbmnet_packer(true);
}

void flib_link_cnet::disable_readout() {
  enable_cbmnet_packer(false);
  rst_pending_mc();
}

//////*** CBMnet control interface ***//////

int flib_link_cnet::send_dcm(const ctrl_msg_t* msg) {

  assert(msg->words >= 4 && msg->words <= 32);

  // check if send FSM is ready (bit 31 in r_ctrl_tx = 0)
  if ((m_rfgtx->get_reg(CNET_REG_GTX_CTRL_TX) & (1 << 31)) != 0) {
    return -1;
  }

  // copy msg to board memory
  size_t bytes = msg->words * 2 + (msg->words * 2) % 4;
  m_rfgtx->set_mem(CNET_MEM_BASE_CTRL_TX, static_cast<const void*>(msg->data),
                   bytes >> 2);

  // start send FSM
  uint32_t ctrl_tx = 0;
  ctrl_tx = 1 << 31 | (msg->words - 1);
  m_rfgtx->set_reg(CNET_REG_GTX_CTRL_TX, ctrl_tx);

  return 0;
}

int flib_link_cnet::recv_dcm(ctrl_msg_t* msg) {

  int ret = 0;
  uint32_t ctrl_rx = m_rfgtx->get_reg(CNET_REG_GTX_CTRL_RX);
  msg->words = (ctrl_rx & 0x1F) + 1;

  // check if a msg is available
  if ((ctrl_rx & (1 << 31)) == 0) {
    return -1;
  }
  // check if received words are in boundary
  // append or truncate if not
  if (msg->words < 4 || msg->words > 32) {
    msg->words = 32;
    ret = -2;
  }

  // read msg from board memory
  size_t bytes = msg->words * 2 + (msg->words * 2) % 4;
  m_rfgtx->get_mem(CNET_MEM_BASE_CTRL_RX, static_cast<void*>(msg->data),
                   bytes >> 2);

  // acknowledge msg
  m_rfgtx->set_reg(CNET_REG_GTX_CTRL_RX, 0);

  return ret;
}

void flib_link_cnet::prepare_dlm(uint8_t type, bool enable) {
  uint32_t reg = 0;
  if (enable) {
    reg = (1 << 4) | (type & 0xF);
  } else {
    reg = (type & 0xF);
  }
  m_rfgtx->set_reg(CNET_REG_GTX_DLM, reg);
  // dummy read on GTX regfile to ensure reg written
  // before issuing send pulse in send_dlm()
  m_rfgtx->get_reg(CNET_REG_GTX_DLM);
}

void flib_link_cnet::send_dlm() {
  // TODO: hack to overcome SW issues
  // DLM send is also avialable in link
  // will send DLMs on all prepared links

  // TODO; HW link could unprepare automatically
  // after one DLM is sent

  // global send
  // m_rfglobal->set_reg(CNET_REG_DLM_CFG, 1);
  // local HW send, not well tested
  m_rfgtx->set_bit(CNET_REG_GTX_DLM, 30, true);
}

uint8_t flib_link_cnet::recv_dlm() {
  // get dlm rx reg
  uint32_t reg = m_rfgtx->get_reg(CNET_REG_GTX_DLM);
  uint8_t type = (reg >> 5) & 0xF;
  // clear dlm rx reg
  m_rfgtx->set_bit(CNET_REG_GTX_DLM, 31, true);
  return type;
}

//////*** CBMnet diagnostics ***//////

uint32_t flib_link_cnet::diag_pcs_startup() {
  return m_rfgtx->get_reg(CNET_REG_GTX_DIAG_PCS_STARTUP);
}

uint32_t flib_link_cnet::diag_ebtb_code_err() {
  return m_rfgtx->get_reg(CNET_REG_GTX_DIAG_EBTB_CODE_ERR);
}

uint32_t flib_link_cnet::diag_ebtb_disp_err() {
  return m_rfgtx->get_reg(CNET_REG_GTX_DIAG_EBTB_DISP_ERR);
}

uint32_t flib_link_cnet::diag_crc_error() {
  return m_rfgtx->get_reg(CNET_REG_GTX_DIAG_CRC_ERROR);
}

uint32_t flib_link_cnet::diag_packet() {
  return m_rfgtx->get_reg(CNET_REG_GTX_DIAG_PACKET);
}

uint32_t flib_link_cnet::diag_packet_err() {
  return m_rfgtx->get_reg(CNET_REG_GTX_DIAG_PACKET_ERR);
}

flib_link_cnet::diag_flags_t flib_link_cnet::diag_flags() {
  uint32_t reg = m_rfgtx->get_reg(CNET_REG_GTX_DIAG_FLAGS);
  diag_flags_t flags;
  flags.pcs_startup = ((reg & (1)) != 0u);
  flags.ebtb_code_err = ((reg & (1 << 1)) != 0u);
  flags.ebtb_disp_err = ((reg & (1 << 2)) != 0u);
  flags.crc_error = ((reg & (1 << 3)) != 0u);
  flags.packet = ((reg & (1 << 4)) != 0u);
  flags.packet_err = ((reg & (1 << 5)) != 0u);
  flags.rx_clk_stable = ((reg & (1 << 6)) != 0u);
  flags.tx_clk_stable = ((reg & (1 << 7)) != 0u);
  flags.ebtb_detect = ((reg & (1 << 8)) != 0u);
  flags.serdes_ready = ((reg & (1 << 9)) != 0u);
  flags.link_active = ((reg & (1 << 10)) != 0u);
  return flags;
}

void flib_link_cnet::diag_clear() {
  m_rfgtx->set_reg(CNET_REG_GTX_DIAG_CLEAR, 0xFFFFFFFF);
}

flib_link_cnet::link_status_t flib_link_cnet::link_status() {
  uint32_t sts = m_rfgtx->get_reg(CNET_REG_GTX_DATAPATH_STS);

  link_status_t link_status;
  link_status.link_active = ((sts & (1)) != 0u);
  link_status.data_rx_stop = ((sts & (1 << 1)) != 0u);
  link_status.ctrl_rx_stop = ((sts & (1 << 2)) != 0u);
  link_status.ctrl_tx_stop = ((sts & (1 << 3)) != 0u);

  return link_status;
}

} // namespace flib
