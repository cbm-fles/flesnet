/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 * Derived from ALICE CRORC Project written by
 * Heiko Engel <hengel@cern.ch>
 *
 */

#pragma once

#include <pda/device.hpp>
#include <pda/pci_bar.hpp>

#include <registers.h>
#include <data_structures.hpp>
#include <dma_channel.hpp>
#include <register_file_bar.hpp>

namespace flib {

class dma_channel;

class flib_link {

public:
  flib_link(size_t link_index, pda::device* dev, pda::pci_bar* bar);
  virtual ~flib_link() = 0;

  void init_dma(void* data_buffer,
                size_t data_buffer_log_size,
                void* desc_buffer,
                size_t desc_buffer_log_size);

  void init_dma(size_t data_buffer_log_size, size_t desc_buffer_log_size);

  void deinit_dma();

  /*** DPB Emualtion ***/
  void init_datapath();
  void reset_datapath();
  void reset_link();

  void set_start_idx(uint64_t index);
  void rst_pending_mc();

  typedef enum {
    rx_disable = 0x0,
    rx_emu = 0x1,
    rx_link = 0x2,
    rx_pgen = 0x3
  } data_sel_t;

  void set_data_sel(data_sel_t rx_sel);
  data_sel_t data_sel();

  typedef struct __attribute__((__packed__)) {
    uint16_t eq_id;  // "Equipment identifier"
    uint8_t sys_id;  // "Subsystem identifier"
    uint8_t sys_ver; // "Subsystem format version"
  } hdr_config_t;

  void set_hdr_config(const hdr_config_t* config);
  uint64_t pending_mc();
  uint64_t mc_index();

  virtual void enable_readout(bool enable) = 0;

  /*** Getter ***/
  size_t link_index() { return m_link_index; };
  pda::device* parent_device() { return m_parent_device; };

  dma_channel* channel() const;
  register_file* register_file_packetizer() const { return m_rfpkt.get(); }
  register_file* register_file_gtx() const { return m_rfgtx.get(); }

protected:
  std::unique_ptr<dma_channel> m_dma_channel;
  std::unique_ptr<register_file> m_rfglobal; // TODO remove this later
  std::unique_ptr<register_file> m_rfpkt;
  std::unique_ptr<register_file> m_rfgtx;

  size_t m_link_index = 0;

  sys_bus_addr m_base_addr;
  pda::device* m_parent_device;
};
}
