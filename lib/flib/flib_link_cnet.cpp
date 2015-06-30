/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <cassert>
#include <memory>

#include <flib_link_cnet.hpp>

namespace flib {

  flib_link_cnet::flib_link_cnet(size_t link_index, pda::device* dev, pda::pci_bar* bar)
    : flib_link(link_index, dev, bar) {}

//////*** CBMnet control interface ***//////

int flib_link_cnet::send_dcm(const ctrl_msg_t* msg) {

  assert(msg->words >= 4 && msg->words <= 32);

  // check if send FSM is ready (bit 31 in r_ctrl_tx = 0)
  if ((m_rfgtx->reg(RORC_REG_GTX_CTRL_TX) & (1 << 31)) != 0) {
    return -1;
  }

  // copy msg to board memory
  size_t bytes = msg->words * 2 + (msg->words * 2) % 4;
  m_rfgtx->set_mem(RORC_MEM_BASE_CTRL_TX, static_cast<const void*>(msg->data), bytes >> 2);

  // start send FSM
  uint32_t ctrl_tx = 0;
  ctrl_tx = 1 << 31 | (msg->words - 1);
  m_rfgtx->set_reg(RORC_REG_GTX_CTRL_TX, ctrl_tx);

  return 0;
}

int flib_link_cnet::recv_dcm(ctrl_msg_t* msg) {

  int ret = 0;
  uint32_t ctrl_rx = m_rfgtx->reg(RORC_REG_GTX_CTRL_RX);
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
  m_rfgtx->mem(RORC_MEM_BASE_CTRL_RX, static_cast<void*>(msg->data), bytes >> 2);

  // acknowledge msg
  m_rfgtx->set_reg(RORC_REG_GTX_CTRL_RX, 0);

  return ret;
}

void flib_link_cnet::prepare_dlm(uint8_t type, bool enable) {
  uint32_t reg = 0;
  if (enable) {
    reg = (1 << 4) | (type & 0xF);
  } else {
    reg = (type & 0xF);
  }
  m_rfgtx->set_reg(RORC_REG_GTX_DLM, reg);
  // dummy read on GTX regfile to ensure reg written
  // before issuing send pulse in send_dlm()
  m_rfgtx->reg(RORC_REG_GTX_DLM);
}

void flib_link_cnet::send_dlm() {
  // TODO: hack to overcome SW issues
  // DLM send is also avialable in link
  // will send DLMs on all prepared links

  // TODO; HW link could unprepare automatically
  // after one DLM is sent

  // global send
  // m_rfglobal->set_reg(RORC_REG_DLM_CFG, 1);
  // local HW send, not well tested
  m_rfgtx->set_bit(RORC_REG_GTX_DLM, 30, true);
}

uint8_t flib_link_cnet::recv_dlm() {
  // get dlm rx reg
  uint32_t reg = m_rfgtx->reg(RORC_REG_GTX_DLM);
  uint8_t type = (reg >> 5) & 0xF;
  // clear dlm rx reg
  m_rfgtx->set_bit(RORC_REG_GTX_DLM, 31, true);
  return type;
}
  
//////*** CBMnet diagnostics ***//////

uint32_t flib_link_cnet::diag_pcs_startup() {
  return m_rfgtx->reg(RORC_REG_GTX_DIAG_PCS_STARTUP);
}

uint32_t flib_link_cnet::diag_ebtb_code_err() {
  return m_rfgtx->reg(RORC_REG_GTX_DIAG_EBTB_CODE_ERR);
}

uint32_t flib_link_cnet::diag_ebtb_disp_err() {
  return m_rfgtx->reg(RORC_REG_GTX_DIAG_EBTB_DISP_ERR);
}

uint32_t flib_link_cnet::diag_crc_error() {
  return m_rfgtx->reg(RORC_REG_GTX_DIAG_CRC_ERROR);
}

uint32_t flib_link_cnet::diag_packet() {
  return m_rfgtx->reg(RORC_REG_GTX_DIAG_PACKET);
}

uint32_t flib_link_cnet::diag_packet_err() {
  return m_rfgtx->reg(RORC_REG_GTX_DIAG_PACKET_ERR);
}

flib_link_cnet::diag_flags_t flib_link_cnet::diag_flags() {
  uint32_t reg =  m_rfgtx->reg(RORC_REG_GTX_DIAG_FLAGS);
  diag_flags_t flags;
  flags.pcs_startup   = (reg & (1));
  flags.ebtb_code_err = (reg & (1 << 1));
  flags.ebtb_disp_err = (reg & (1 << 2));
  flags.crc_error     = (reg & (1 << 3));
  flags.packet        = (reg & (1 << 4));
  flags.packet_err    = (reg & (1 << 5));
  flags.rx_clk_stable = (reg & (1 << 6));
  flags.tx_clk_stable = (reg & (1 << 7));
  flags.ebtb_detect   = (reg & (1 << 8));
  flags.serdes_ready  = (reg & (1 << 9));
  flags.link_active   = (reg & (1 << 10));
  return flags;
}

void flib_link_cnet::diag_clear() {
  m_rfgtx->set_reg(RORC_REG_GTX_DIAG_CLEAR, 0xFFFFFFFF);
}

flib_link_cnet::link_status_t flib_link_cnet::link_status() {
  uint32_t sts = m_rfgtx->reg(RORC_REG_GTX_DATAPATH_STS);

  link_status_t link_status;
  link_status.link_active = (sts & (1));
  link_status.data_rx_stop = (sts & (1 << 1));
  link_status.ctrl_rx_stop = (sts & (1 << 2));
  link_status.ctrl_tx_stop = (sts & (1 << 3));

  return link_status;
}
  
} // namespace
