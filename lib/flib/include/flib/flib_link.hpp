#ifndef FLIB_LINK_HPP
#define FLIB_LINK_HPP

#include <flib/rorc_registers.h>
#include <flib/rorcfs_bar.hh>
#include <flib/rorcfs_device.hh>
#include <flib/rorcfs_buffer.hh>
#include <flib/rorcfs_dma_channel.hh>
#include <flib/register_file_bar.hpp>

#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace flib {
  
// has to be 256 Bit, this is hard coded in hw
struct __attribute__ ((__packed__))
MicrosliceDescriptor
{
  uint8_t   hdr_id;  // "Header format identifier" DD
  uint8_t   hdr_ver; // "Header format version"    01
  uint16_t  eq_id;   // "Equipment identifier"
  uint16_t  flags;   // "Status and error flags"
  uint8_t   sys_id;  // "Subsystem identifier"
  uint8_t   sys_ver; // "Subsystem format version"
  uint64_t  idx;     // "Microslice index"
  uint32_t  crc;     // "CRC32 checksum"
  uint32_t  size;    // "Size bytes"
  uint64_t  offset;  // "Ofsset in event buffer"
};

struct __attribute__ ((__packed__))
hdr_config
{
  uint16_t  eq_id;   // "Equipment identifier"
  uint8_t   sys_id;  // "Subsystem identifier"
  uint8_t   sys_ver; // "Subsystem format version"
};

struct mc_desc
{
    uint64_t nr;
    volatile uint64_t* addr;
    uint32_t size; // bytes
    volatile uint64_t* rbaddr;
};

struct ctrl_msg
{
  uint32_t words; // num 16 bit data words
  uint16_t data[32];
};

// Tags to indicate mode of buffer initialization
struct create_only_t {};
struct open_only_t {};
struct open_or_create_t {};
 
static const create_only_t    create_only    = create_only_t();
static const open_only_t      open_only      = open_only_t();
static const open_or_create_t open_or_create = open_or_create_t();
  
class FlibException : public std::runtime_error
{
public:

  explicit FlibException(const std::string& what_arg = "")
    : std::runtime_error(what_arg) { }
};

class RorcfsException : public FlibException
{
public:

  explicit RorcfsException(const std::string& what_arg = "")
    : FlibException(what_arg) { }
};

class flib_link
{
public:
  
  flib_link(size_t link_index, device* dev, rorcfs_bar* bar)
    : m_link_index(link_index), m_device(dev) {

    m_base_addr =  (m_link_index + 1) * RORC_CHANNEL_OFFSET;
    // regiter file access
    m_rfpkt = std::unique_ptr<register_file_bar>(
       new register_file_bar(bar, m_base_addr));
    m_rfgtx = std::unique_ptr<register_file_bar>(
       new register_file_bar(bar, (m_base_addr + (1<<RORC_DMA_CMP_SEL))));
    m_rfglobal = std::unique_ptr<register_file_bar>(
       new register_file_bar(bar, 0));
    // create DMA channel and bind to register file, 
    // no HW initialization is done here
    m_channel = std::unique_ptr<rorcfs_dma_channel>(
       new rorcfs_dma_channel(m_rfpkt.get() ));
    
  }

  ~flib_link() {
    stop();
    //TODO move deallocte to destructro of buffer
    if(m_event_buffer){
      if(m_event_buffer->deallocate() != 0) {
        throw RorcfsException("ebuf->deallocate failed");
      }
    }
    if(m_dbuffer){
      if(m_dbuffer->deallocate() != 0) {
        throw RorcfsException("dbuf->deallocate failed");
      }
    }
  }  
  
  int init_dma(create_only_t, size_t log_ebufsize, size_t log_dbufsize) {
    m_log_ebufsize = log_ebufsize;
    m_log_dbufsize = log_dbufsize;
    m_event_buffer = _create_buffer(0, log_ebufsize);
    m_dbuffer = _create_buffer(1, log_dbufsize);
    init_hardware();
    m_dma_initialized = true;
    return 0;
  }

  int init_dma(open_only_t, size_t log_ebufsize, size_t log_dbufsize) {
    m_log_ebufsize = log_ebufsize;
    m_log_dbufsize = log_dbufsize;
    m_event_buffer = _open_buffer(0);
    m_dbuffer = _open_buffer(1);
    init_hardware();
    m_dma_initialized = true;
    return 0;
  }
 
  int init_dma(open_or_create_t, size_t log_ebufsize, size_t log_dbufsize) {
    m_log_ebufsize = log_ebufsize;
    m_log_dbufsize = log_dbufsize;
    m_event_buffer = _open_or_create_buffer(0, log_ebufsize);
    m_dbuffer = _open_or_create_buffer(1, log_dbufsize);
    init_hardware();
    m_dma_initialized = true;
    return 0;
  }

  ///// MC access funtions /////

  std::pair<mc_desc, bool> get_mc() {
    struct mc_desc mc;
    if(m_db[m_index].idx > m_mc_nr) { // mc_nr counts from 1 in HW
      m_mc_nr = m_db[m_index].idx;
      mc.nr = m_mc_nr;
      mc.addr = m_eb + (m_db[m_index].offset & ((1<<m_log_ebufsize)-1))/sizeof(uint64_t);
      mc.size = m_db[m_index].size;
      mc.rbaddr = (uint64_t *)&m_db[m_index];
      
      // calculate next rb index
      m_last_index = m_index;
      if( m_index < m_dbentries-1 )
        m_index++;
      else {
        m_wrap++;
        m_index = 0;
      }
      return std::make_pair(mc, true);
    }
    else
      return std::make_pair(mc, false);
  }
  
  int ack_mc() {
    
    // TODO: EB pointers are set to begin of acknoledged entry, pointers are one entry delayed
    // to calculate end wrapping logic is required
    uint64_t eb_offset = m_db[m_last_index].offset & ((1<<m_log_ebufsize)-1);
    // each rbenty is 32 bytes, this is hard coded in HW
    uint64_t rb_offset = m_last_index*sizeof(struct MicrosliceDescriptor) & ((1<<m_log_dbufsize)-1);

    //_ch->setEBOffset(eb_offset);
    //_ch->setRBOffset(rb_offset);

    m_channel->setOffsets(eb_offset, rb_offset);

#ifdef DEBUG  
    printf("index %d EB offset set: %ld, get: %ld\n",
           m_last_index, eb_offset, m_channel->getEBOffset());
    printf("index %d RB offset set: %ld, get: %ld, wrap %d\n",
           m_last_index, rb_offset, m_channel->getRBOffset(), m_wrap);
#endif

    return 0;
    }
  
  ///// configuration and control /////

  // REG: mc_gen_cfg
  // bit 0 set_start_index
  // bit 1 rst_pending_mc
  // bit 2 packer enable

  void set_start_idx(uint64_t index) {
    // set reset value
    // TODO replace with _rfgtx->set_mem()
    m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG_IDX_L, (uint32_t)(index & 0xffffffff));
    m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG_IDX_H, (uint32_t)(index >> 32));
    // reste mc counter 
    // TODO implenet edge detection and 'pulse only' in HW
    uint32_t mc_gen_cfg= m_rfgtx->get_reg(RORC_REG_GTX_MC_GEN_CFG);
    // TODO replace with _rfgtx->set_bit()
    m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | 1));
    m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1)));
  }
  
  void rst_pending_mc() { // is also resetted with datapath reset
    // TODO implenet edge detection and 'pulse only' in HW
    uint32_t mc_gen_cfg= m_rfgtx->get_reg(RORC_REG_GTX_MC_GEN_CFG);
    // TODO replace with _rfgtx->set_bit()
    m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | (1<<1)));
    m_rfgtx->set_reg(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1<<1)));
  }
  
  void enable_cbmnet_packer(bool enable) {
    m_rfgtx->set_bit(RORC_REG_GTX_MC_GEN_CFG, 2, enable);
  }

  uint64_t get_pending_mc() {
    // TODO replace with _rfgtx->get_mem()
    uint64_t pend_mc = m_rfgtx->get_reg(RORC_REG_GTX_PENDING_MC_L);
    pend_mc = pend_mc | ((uint64_t)(m_rfgtx->get_reg(RORC_REG_GTX_PENDING_MC_H))<<32);
    return pend_mc;
  }

  uint64_t get_mc_index() {
    // TODO replace with _rfgtx->get_mem()
    uint64_t mc_index = m_rfgtx->get_reg(RORC_REG_GTX_MC_INDEX_L);
    mc_index = mc_index | ((uint64_t)(m_rfgtx->get_reg(RORC_REG_GTX_MC_INDEX_H))<<32);
    return mc_index;
  }
  
  // REG: datapath_cfg
  // bit 0-1 data_rx_sel (10: link, 11: pgen, 01: emu, 00: disable)
  enum data_rx_sel {disable, emu, link, pgen};

  void set_data_rx_sel(data_rx_sel rx_sel) {
    uint32_t dp_cfg = m_rfgtx->get_reg(RORC_REG_GTX_DATAPATH_CFG);
    switch (rx_sel) {
    case disable : m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg & ~3)); break;
    case link :    m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | (1<<1)) & ~1) ); break;
    case pgen :    m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg | 3) ); break;
    case emu :     m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | 1)) & ~(1<<1)); break;
    }
  }

  data_rx_sel get_data_rx_sel() {
    uint32_t dp_cfg = m_rfgtx->get_reg(RORC_REG_GTX_DATAPATH_CFG);
    return static_cast<data_rx_sel>(dp_cfg & 0x3);
  }

  void set_hdr_config(const struct hdr_config* config) {
    m_rfgtx->set_mem(RORC_REG_GTX_MC_GEN_CFG_HDR, (const void*)config, sizeof(hdr_config)>>2);
  }


  ///// CBMnet control interface /////
  
  int send_dcm(const struct ctrl_msg* msg) {
    // TODO: could also implement blocking call 
    //       and check if sending is done at the end

    assert (msg->words >= 4 && msg->words <= 32);
    
    // check if send FSM is ready (bit 31 in r_ctrl_tx = 0)
    if ( (m_rfgtx->get_reg(RORC_REG_GTX_CTRL_TX) & (1<<31)) != 0 ) {
      return -1;
    }
    
    // copy msg to board memory
    size_t bytes = msg->words*2 + (msg->words*2)%4;
    m_rfgtx->set_mem(RORC_MEM_BASE_CTRL_TX, (const void*)msg->data, bytes>>2);
    
    // start send FSM
    uint32_t ctrl_tx = 0;
    ctrl_tx = 1<<31 | (msg->words-1);
    m_rfgtx->set_reg(RORC_REG_GTX_CTRL_TX, ctrl_tx);
    
    return 0;
  }

  int recv_dcm(struct ctrl_msg* msg) {
    
    int ret = 0;
    uint32_t ctrl_rx = m_rfgtx->get_reg(RORC_REG_GTX_CTRL_RX);
    msg->words = (ctrl_rx & 0x1F)+1;
    
    // check if a msg is available
    if ((ctrl_rx & (1<<31)) == 0) {
      return -1;
    }
    // check if received words are in boundary
    // append or truncate if not
    if (msg->words < 4 || msg->words > 32) {
      msg->words = 32;
      ret = -2;
    }
    
    // read msg from board memory
    size_t bytes = msg->words*2 + (msg->words*2)%4;
    m_rfgtx->get_mem(RORC_MEM_BASE_CTRL_RX, (void*)msg->data, bytes>>2);
    
    // acknowledge msg
    m_rfgtx->set_reg(RORC_REG_GTX_CTRL_RX, 0);
    
    return ret;
  } 

  // RORC_REG_DLM_CFG 0 set to send (global reg)
  // RORC_REG_GTX_DLM 3..0 tx type, 4 enable,
  //                  8..5 rx type, 31 set to clear rx reg
  void prepare_dlm(uint8_t type, bool enable) {
    uint32_t reg = 0;
    if (enable)
      reg = (1<<4) | (type & 0xF);
    else
      reg = (type & 0xF);
    m_rfgtx->set_reg(RORC_REG_GTX_DLM, reg);
  }

  void send_dlm() {
    // TODO implemet local register in HW
    // this causes all prepared links to send
    m_rfglobal->set_reg(RORC_REG_DLM_CFG, 1);
  }

  uint8_t recv_dlm() {
    // get dlm rx reg
    uint32_t reg = m_rfgtx->get_reg(RORC_REG_GTX_DLM);
    uint8_t type = (reg>>5) & 0xF;
    // clear dlm rx reg
    m_rfgtx->set_bit(RORC_REG_GTX_DLM, 31, true);
    return type;
  }

  ///// getter funtions /////
  
  std::string get_ebuf_info() {
    return get_buffer_info(m_event_buffer.get());
  }
  
  std::string get_dbuf_info() {
    return get_buffer_info(m_dbuffer.get());
  }

  rorcfs_buffer* ebuf() const {
    return m_event_buffer.get();
  }
  
  rorcfs_buffer* rbuf() const {
    return m_dbuffer.get();
  }
  
  rorcfs_dma_channel* get_ch() const {
    return m_channel.get();
  }

  register_file_bar* get_rfpkt() const {
    return m_rfpkt.get();
  }

  register_file_bar* get_rfgtx() const {
    return m_rfgtx.get();
  }

protected:
    std::unique_ptr<rorcfs_dma_channel> m_channel;
    std::unique_ptr<rorcfs_buffer>      m_event_buffer;
    std::unique_ptr<rorcfs_buffer>      m_dbuffer;
    std::unique_ptr<register_file_bar>  m_rfglobal; // TODO remove this later
    std::unique_ptr<register_file_bar>  m_rfpkt;
    std::unique_ptr<register_file_bar>  m_rfgtx;

    size_t   m_link_index   = 0;
    size_t   m_log_ebufsize = 0;
    size_t   m_log_dbufsize = 0;

    uint64_t m_index        = 0;
    uint64_t m_last_index   = 0;
    uint64_t m_last_acked   = 0;
    uint64_t m_mc_nr        = 0;
    uint64_t m_wrap         = 0;
    uint64_t m_dbentries    = 0;

    bool m_dma_initialized = false;

    sys_bus_addr  m_base_addr;
    device       *m_device;

    volatile uint64_t                    *m_eb = nullptr;
    volatile struct MicrosliceDescriptor *m_db = nullptr;




    // creates new buffer, throws an exception if buffer already exists
    std::unique_ptr<rorcfs_buffer>
    _create_buffer(size_t idx, size_t log_size)
    {
        unsigned long size = (((unsigned long)1) << log_size);
        std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());
        if (buffer->allocate(m_device, size, 2*m_link_index+idx, 1, RORCFS_DMA_FROM_DEVICE)!=0)
        {
            if (errno == EEXIST)
            { throw FlibException("Buffer already exists, not allowed to open in create only mode"); }
            else
            { throw RorcfsException("Buffer allocation failed"); }
        }
        return buffer;
    }

  // opens an existing buffer, throws an exception if buffer doesn't exists
  std::unique_ptr<rorcfs_buffer> _open_buffer(size_t idx) {
    std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());			
    if ( buffer->connect(m_device, 2*m_link_index+idx) != 0 ) {
      throw RorcfsException("Connect to buffer failed");
    }
    return buffer;    
  }

  // opens existing buffer or creates new buffer if non exists
  std::unique_ptr<rorcfs_buffer> _open_or_create_buffer(size_t idx, size_t log_size) {
    unsigned long size = (((unsigned long)1) << log_size);
    std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());			
    if (buffer->allocate(m_device, size, 2*m_link_index+idx, 1, RORCFS_DMA_FROM_DEVICE)!=0) {
      if (errno == EEXIST) {
        if ( buffer->connect(m_device, 2*m_link_index+idx) != 0 )
          throw RorcfsException("Buffer open failed");
      }
      else
        throw RorcfsException("Buffer allocation failed");
      }
    return buffer;
  }

  void _rst_channel() {
    // datapath reset, will also cause hw defaults for
    // - pending mc  = 0
    m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, true);
     // rst packetizer fifos
    m_channel->setDMAConfig(0X2);
    // release datapath reset
    m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, false);
  }

    void stop();

    /** Initializes hardware to perform DMA transfers
     **/
    int init_hardware();

    std::string get_buffer_info(rorcfs_buffer* buf);

};
} // namespace flib

#pragma GCC diagnostic pop

#endif // FLIB_LINK_HPP
