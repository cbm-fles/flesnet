/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#pragma once

#include <flib_link.hpp>

namespace flib {

class flib_link_cnet : public flib_link {

public:

  flib_link_cnet(size_t link_index, pda::device* dev, pda::pci_bar* bar);

  /*** CBMnet control interface ***/
  typedef struct {
    uint32_t words; // num 16 bit data words
    uint16_t data[32];
  } ctrl_msg_t ;

  int send_dcm(const ctrl_msg_t* msg);
  int recv_dcm(ctrl_msg_t* msg);
  void prepare_dlm(uint8_t type, bool enable);
  void send_dlm();
  uint8_t recv_dlm();
  
  /*** CBMnet diagnostics ***/
  typedef struct {
    bool pcs_startup;
    bool ebtb_code_err;
    bool ebtb_disp_err;
    bool crc_error;
    bool packet;
    bool packet_err;
    bool rx_clk_stable;
    bool tx_clk_stable;
    bool ebtb_detect;
    bool serdes_ready;
    bool link_active;
  } diag_flags_t;

  uint32_t diag_pcs_startup();
  uint32_t diag_ebtb_code_err();
  uint32_t diag_ebtb_disp_err();
  uint32_t diag_crc_error();
  uint32_t diag_packet();
  uint32_t diag_packet_err();
  // read all flags
  diag_flags_t diag_flags();
  // clear all counters
  void diag_clear();

  typedef struct {
    bool link_active;
    bool data_rx_stop;
    bool ctrl_rx_stop;
    bool ctrl_tx_stop;
  } link_status_t;

  link_status_t link_status();

};
}
