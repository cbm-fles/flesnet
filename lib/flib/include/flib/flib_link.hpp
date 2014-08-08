/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>, Dominic Eschweiler
 * <dominic.eschweiler@cern.ch>
 * @version 0.1
 * @date 2014-07-08
 */

#ifndef FLIB_LINK_HPP
#define FLIB_LINK_HPP

#include <flib/registers.h>
#include <flib/data_structures.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace flib {
// Tags to indicate mode of buffer initialization
struct create_only_t {};
struct open_only_t {};
struct open_or_create_t {};

static const create_only_t create_only = create_only_t();
static const open_only_t open_only = open_only_t();
static const open_or_create_t open_or_create = open_or_create_t();

class device;
class pci_bar;
class dma_buffer;
class dma_channel;
class register_file_bar;

class flib_link {
public:
  flib_link(size_t link_index, device* dev, pci_bar* bar);
  ~flib_link();

  int init_dma(create_only_t, size_t log_ebufsize, size_t log_dbufsize);

  int init_dma(open_only_t, size_t log_ebufsize, size_t log_dbufsize);

  int init_dma(open_or_create_t, size_t log_ebufsize, size_t log_dbufsize);

  /*** MC access funtions ***/
  std::pair<mc_desc, bool> get_mc();
  int ack_mc();

  /*** Configuration and control ***/

  /**
   * REG: mc_gen_cfg
   * bit 0 set_start_index
   * bit 1 rst_pending_mc
   * bit 2 packer enable
   */
  void set_start_idx(uint64_t index);
  void rst_pending_mc();
  void rst_cnet_link();
  void enable_cbmnet_packer(bool enable);

  /*** CBMnet control interface ***/

  int send_dcm(const struct ctrl_msg* msg);
  int recv_dcm(struct ctrl_msg* msg);

  /**
   * RORC_REG_DLM_CFG 0 set to send (global reg)
   * RORC_REG_GTX_DLM 3..0 tx type, 4 enable,
   * 8..5 rx type, 31 set to clear rx reg
   */
  void prepare_dlm(uint8_t type, bool enable);
  void send_dlm();
  uint8_t recv_dlm();

  /*** setter methods ***/
  // REG: datapath_cfg
  // bit 1-0 data_rx_sel (10: link, 11: pgen, 01: emu, 00: disable)
  enum data_rx_sel { rx_disable, rx_emu, rx_link, rx_pgen };
  void set_data_rx_sel(data_rx_sel rx_sel);
  void set_hdr_config(const struct hdr_config* config);

  /*** getter methods ***/
  uint64_t get_pending_mc();
  uint64_t get_mc_index();
  data_rx_sel get_data_rx_sel();
  std::string get_ebuf_info();
  std::string get_dbuf_info();
  dma_buffer* ebuf() const;
  dma_buffer* rbuf() const;
  dma_channel* get_ch() const;
  register_file_bar* get_rfpkt() const;
  register_file_bar* get_rfgtx() const;

  struct link_status {
    bool link_active;
    bool data_rx_stop;
    bool ctrl_rx_stop;
    bool ctrl_tx_stop;
  };

  struct link_status get_link_status();

protected:
  std::unique_ptr<dma_channel> m_channel;
  std::unique_ptr<dma_buffer> m_event_buffer;
  std::unique_ptr<dma_buffer> m_dbuffer;
  std::unique_ptr<register_file_bar> m_rfglobal; // TODO remove this later
  std::unique_ptr<register_file_bar> m_rfpkt;
  std::unique_ptr<register_file_bar> m_rfgtx;

  size_t m_link_index = 0;
  size_t m_log_ebufsize = 0;
  size_t m_log_dbufsize = 0;

  uint64_t m_index = 0;
  uint64_t m_last_index = 0;
  uint64_t m_last_acked = 0;
  uint64_t m_mc_nr = 0;
  uint64_t m_wrap = 0;
  uint64_t m_dbentries = 0;

  bool m_dma_initialized = false;

  sys_bus_addr m_base_addr;
  device* m_device;

  volatile uint64_t* m_eb = nullptr;
  volatile struct MicrosliceDescriptor* m_db = nullptr;

  /**
   * Creates new buffer, throws an exception if buffer already exists
   */
  std::unique_ptr<dma_buffer> create_buffer(size_t idx, size_t log_size);

  /**
   * Opens an existing buffer, throws an exception if buffer
   * doesn't exists
   */
  std::unique_ptr<dma_buffer> open_buffer(size_t idx);

  /**
   * Opens an existing buffer or creates new buffer if non exists
   */
  std::unique_ptr<dma_buffer> open_or_create_buffer(size_t idx,
                                                    size_t log_size);

  void reset_channel();
  void stop();

  /**
   * Initializes hardware to perform DMA transfers
   */
  int init_hardware();

  std::string get_buffer_info(dma_buffer* buf);
};
}

#pragma GCC diagnostic pop

#endif /** FLIB_LINK_HPP */
