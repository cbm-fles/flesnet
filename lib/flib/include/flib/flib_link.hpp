#ifndef FLIB_LINK_HPP
#define FLIB_LINK_HPP

#include "rorc_registers.h"
#include "rorcfs_bar.hh"
#include "rorcfs_device.hh"
#include "rorcfs_buffer.hh"
#include "rorcfs_dma_channel.hh"
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace flib {
  
// has to be 256 Bit, this is hard coded in hw
struct __attribute__ ((__packed__)) MicrosliceDescriptor {
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

struct __attribute__ ((__packed__)) hdr_config {
  uint16_t  eq_id;   // "Equipment identifier"
  uint8_t   sys_id;  // "Subsystem identifier"
  uint8_t   sys_ver; // "Subsystem format version"
};

struct mc_desc {
    uint64_t nr;
    volatile uint64_t* addr;
    uint32_t size; // bytes
    volatile uint64_t* rbaddr;
};

struct ctrl_msg {
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
  
class FlibException : public std::runtime_error {
public:

  explicit FlibException(const std::string& what_arg = "")
    : std::runtime_error(what_arg) { }
};

class RorcfsException : public FlibException {
public:

  explicit RorcfsException(const std::string& what_arg = "")
    : FlibException(what_arg) { }
};

class flib_link {
  
  std::unique_ptr<rorcfs_dma_channel> _ch;
  std::unique_ptr<rorcfs_buffer> _ebuf;
  std::unique_ptr<rorcfs_buffer> _dbuf;
  rorcfs_device* _dev;
  uint8_t _channel;

  uint64_t _index = 0;
  uint64_t _last_index = 0;
  uint64_t _last_acked = 0;
  uint64_t _mc_nr = 0;
  uint64_t _wrap = 0;
  bool _dma_initialized = false;
  
  volatile uint64_t* _eb = nullptr;
  volatile struct MicrosliceDescriptor* _db = nullptr;
  
  uint64_t _dbentries = 0;
  size_t _log_ebufsize = 0;
  size_t _log_dbufsize = 0;

  
public:
  
  flib_link(uint8_t channel, rorcfs_device* dev, rorcfs_bar* bar) 
    : _dev(dev), _channel(channel) {

    // create DMA channel and bind to BAR1, no HW initialization is done here
    _ch = std::unique_ptr<rorcfs_dma_channel>(new rorcfs_dma_channel());
    _ch->init(bar, (_channel+1)*RORC_CHANNEL_OFFSET);
  }

  ~flib_link() {
    _stop();
    //TODO move deallocte to destructro of buffer
    if(_ebuf){
      if(_ebuf->deallocate() != 0) {
        throw RorcfsException("ebuf->deallocate failed");
      }
    }
    if(_dbuf){
      if(_dbuf->deallocate() != 0) {
        throw RorcfsException("dbuf->deallocate failed");
      }
    }
  }  
  
  int init_dma(create_only_t, size_t log_ebufsize, size_t log_dbufsize) {
    _log_ebufsize = log_ebufsize;
    _log_dbufsize = log_dbufsize;
    _ebuf = _create_buffer(0, log_ebufsize);
    _dbuf = _create_buffer(1, log_dbufsize);
    _init_hardware();
    _dma_initialized = true;
    return 0;
  }

  int init_dma(open_only_t, size_t log_ebufsize, size_t log_dbufsize) {
    _log_ebufsize = log_ebufsize;
    _log_dbufsize = log_dbufsize;
    _ebuf = _open_buffer(0);
    _dbuf = _open_buffer(1);
    _init_hardware();
    _dma_initialized = true;
    return 0;
  }
 
  int init_dma(open_or_create_t, size_t log_ebufsize, size_t log_dbufsize) {
    _log_ebufsize = log_ebufsize;
    _log_dbufsize = log_dbufsize;
    _ebuf = _open_or_create_buffer(0, log_ebufsize);
    _dbuf = _open_or_create_buffer(1, log_dbufsize);
    _init_hardware();
    _dma_initialized = true;
    return 0;
  }

  ///// MC access funtions /////

  std::pair<mc_desc, bool> get_mc() {
    struct mc_desc mc;
    if(_db[_index].idx > _mc_nr) { // mc_nr counts from 1 in HW
      _mc_nr = _db[_index].idx;
      mc.nr = _mc_nr;
      mc.addr = _eb + (_db[_index].offset & ((1<<_log_ebufsize)-1))/sizeof(uint64_t);
      mc.size = _db[_index].size;
      mc.rbaddr = (uint64_t *)&_db[_index];
      
      // calculate next rb index
      _last_index = _index;
      if( _index < _dbentries-1 ) 
        _index++;
      else {
        _wrap++;
        _index = 0;
      }
      return std::make_pair(mc, true);
    }
    else
      return std::make_pair(mc, false);
  }
  
  int ack_mc() {
    
    // TODO: EB pointers are set to begin of acknoledged entry, pointers are one entry delayed
    // to calculate end wrapping logic is required
    uint64_t eb_offset = _db[_last_index].offset & ((1<<_log_ebufsize)-1);
    // each rbenty is 32 bytes, this is hard coded in HW
    uint64_t rb_offset = _last_index*sizeof(struct MicrosliceDescriptor) & ((1<<_log_dbufsize)-1);

    //_ch->setEBOffset(eb_offset);
    //_ch->setRBOffset(rb_offset);

    _ch->setOffsets(eb_offset, rb_offset);

#ifdef DEBUG  
    printf("index %d EB offset set: %ld, get: %ld\n",
           _last_index, eb_offset, _ch->getEBOffset());
    printf("index %d RB offset set: %ld, get: %ld, wrap %d\n",
           _last_index, rb_offset, _ch->getRBOffset(), _wrap);
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
    _ch->setGTX(RORC_REG_GTX_MC_GEN_CFG_IDX_L, (uint32_t)(index & 0xffffffff));
    _ch->setGTX(RORC_REG_GTX_MC_GEN_CFG_IDX_H, (uint32_t)(index >> 32));
    // reste mc counter 
    // TODO implenet edge detection and 'pulse only' in HW
    uint32_t mc_gen_cfg= _ch->getGTX(RORC_REG_GTX_MC_GEN_CFG);
    _ch->setGTX(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | 1));
    _ch->setGTX(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1)));
  }

  void rst_pending_mc() { // is also resetted with datapath reset
    // TODO implenet edge detection and 'pulse only' in HW
    uint32_t mc_gen_cfg= _ch->getGTX(RORC_REG_GTX_MC_GEN_CFG);
    _ch->setGTX(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg | (1<<1)));
    _ch->setGTX(RORC_REG_GTX_MC_GEN_CFG, (mc_gen_cfg & ~(1<<1)));
  }
  
  void enable_cbmnet_packer(bool enable) {
    _ch->set_bitGTX(RORC_REG_GTX_MC_GEN_CFG, 2, enable);
  }

  uint64_t get_pending_mc() {
    uint64_t pend_mc = _ch->getGTX(RORC_REG_GTX_PENDING_MC_L);
    pend_mc = pend_mc | ((uint64_t)(_ch->getGTX(RORC_REG_GTX_PENDING_MC_H))<<32);
    return pend_mc;
  }

  uint64_t get_mc_index() {
    uint64_t mc_index = _ch->getGTX(RORC_REG_GTX_MC_INDEX_L);
    mc_index = mc_index | ((uint64_t)(_ch->getGTX(RORC_REG_GTX_MC_INDEX_H))<<32);
    return mc_index;
  }
  
  // REG: datapath_cfg
  // bit 0-1 data_rx_sel (10: link, 11: pgen, 01: emu, 00: disable)
  enum data_rx_sel {disable, link, pgen, emu};

  void set_data_rx_sel(data_rx_sel rx_sel) {
    uint32_t dp_cfg = _ch->getGTX(RORC_REG_GTX_DATAPATH_CFG);
    switch (rx_sel) {
    case disable : _ch->setGTX(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg & ~3)); break;
    case link :    _ch->setGTX(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | (1<<1)) & ~1) ); break;
    case pgen :    _ch->setGTX(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg | 3) ); break;
    case emu :     _ch->setGTX(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | 1)) & ~(1<<1)); break;
    }
  }

  void set_hdr_config(struct hdr_config* config) {
    _ch->set_memGTX(RORC_REG_GTX_MC_GEN_CFG_HDR, (const void*)config, sizeof(hdr_config));
  }

  ///// CBMnet control interface /////
  
  int send_msg(const struct ctrl_msg* msg) {
    // TODO: could also implement blocking call 
    //       and check if sending is done at the end

    assert (msg->words >= 4 && msg->words <= 32);
    
    // check if send FSM is ready (bit 31 in r_ctrl_tx = 0)
    if ( (_ch->getGTX(RORC_REG_GTX_CTRL_TX) & (1<<31)) != 0 ) {
      return -1;
    }
    
    // copy msg to board memory
    size_t bytes = msg->words*2 + (msg->words*2)%4;
    _ch->set_memGTX(RORC_MEM_BASE_CTRL_TX, (const void*)msg->data, bytes);
    
    // start send FSM
    uint32_t ctrl_tx = 0;
    ctrl_tx = 1<<31 | (msg->words-1);
    _ch->setGTX(RORC_REG_GTX_CTRL_TX, ctrl_tx);
    
    return 0;
  }

  int rcv_msg(struct ctrl_msg* msg) {
    
    int ret = 0;
    uint32_t ctrl_rx = _ch->getGTX(RORC_REG_GTX_CTRL_RX);
    msg->words = (ctrl_rx & 0x1F)+1;
    
    // check if received words are in boundary
    if (msg->words < 4 || msg->words > 32) {
      msg->words = 32;
      ret = -2;
    }
    // check if a msg is available
    if ((ctrl_rx & (1<<31)) == 0) {
      return -1;
    }
    
    // read msg from board memory
    size_t bytes = msg->words*2 + (msg->words*2)%4;
    _ch->get_memGTX(RORC_MEM_BASE_CTRL_RX, (void*)msg->data, bytes);
    
    // acknowledge msg
    _ch->setGTX(RORC_REG_GTX_CTRL_RX, 0);
    
    return ret;
  } 

  // RORC_REG_DLM_CFG 0 set to send
  // RORC_REG_GTX_DLM 3..0 tx type, 4 enable,
  //                  8..5 rx type, 31 set to clear rx reg
  void set_dlm_cfg(uint8_t type, bool enable) {
    uint32_t reg = 0;
    if (enable)
      reg = (1<<4) | (type & 0xF);
    else
      reg = (type & 0xF);
    _ch->setGTX(RORC_REG_GTX_DLM, reg);
  }

  uint8_t get_dlm() {
    uint32_t reg = _ch->getGTX(RORC_REG_GTX_DLM);
    uint8_t type = (reg>>5) & 0xF;
    return type;
  }
  
  void clr_dlm() {
    _ch->setGTX(RORC_REG_GTX_DLM, 1<<31);
  }

  ///// getter funtions /////
  
  std::string get_ebuf_info() {
    return _get_buffer_info(_ebuf.get());
  }
  
  std::string get_dbuf_info() {
    return _get_buffer_info(_dbuf.get());
  }

  rorcfs_buffer* ebuf() const {
    return _ebuf.get();
  }
  
  rorcfs_buffer* rbuf() const {
    return _dbuf.get();
  }
  
  rorcfs_dma_channel* get_ch() const {
    return _ch.get();
  }

private:

  // creates new buffer, throws an exception if buffer already exists
  std::unique_ptr<rorcfs_buffer> _create_buffer(size_t idx, size_t log_size) {
    unsigned long size = (((unsigned long)1) << log_size);
    std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());			
    if (buffer->allocate(_dev, size, 2*_channel+idx, 1, RORCFS_DMA_FROM_DEVICE)!=0) {
      if (errno == EEXIST)
        throw FlibException("Buffer already exists, not allowed to open in create only mode");
      else
        throw RorcfsException("Buffer allocation failed");
    }
    return buffer;
  }

  // opens an existing buffer, throws an exception if buffer doesn't exists
  std::unique_ptr<rorcfs_buffer> _open_buffer(size_t idx) {
    std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());			
    if ( buffer->connect(_dev, 2*_channel+idx) != 0 ) {
      throw RorcfsException("Connect to buffer failed");
    }
    return buffer;    
  }

  // opens existing buffer or creates new buffer if non exists
  std::unique_ptr<rorcfs_buffer> _open_or_create_buffer(size_t idx, size_t log_size) {
    unsigned long size = (((unsigned long)1) << log_size);
    std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());			
    if (buffer->allocate(_dev, size, 2*_channel+idx, 1, RORCFS_DMA_FROM_DEVICE)!=0) {
      if (errno == EEXIST) {
        if ( buffer->connect(_dev, 2*_channel+idx) != 0 )
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
    _ch->set_bitGTX(RORC_REG_GTX_DATAPATH_CFG, 2, true);
     // rst packetizer fifos
    _ch->setDMAConfig(0X2);
    // release datapath reset
    _ch->set_bitGTX(RORC_REG_GTX_DATAPATH_CFG, 2, false);
  }

  void _stop() {
    if(_ch && _dma_initialized ) {
      // disable packer
      enable_cbmnet_packer(false);
      // disable DMA Engine
      _ch->setEnableEB(0);
      // wait for pending transfers to complete (dma_busy->0)
      while( _ch->getDMABusy() ) {
        usleep(100);
      }
      // disable RBDM
      _ch->setEnableRB(0);
      // reset
      _rst_channel();
    }
  }

  // initializes hardware to perform DMA transfers
  int _init_hardware() {
    // disable packer if still enabled
    enable_cbmnet_packer(false);
    // reset everything to ensure clean startup
    _rst_channel();
    set_start_idx(1);
    // prepare EventBufferDescriptorManager
    // and ReportBufferDescriptorManage
    // with scatter-gather list
    if( _ch->prepareEB(_ebuf.get()) < 0 ) {
      return -1;
    }
    if( _ch->prepareRB(_dbuf.get()) < 0 ) {
      return -1;
    }
    
    if( _ch->configureChannel(_ebuf.get(), _dbuf.get(), 128) < 0) {
      return -1;
    }
    
    // clear eb for debugging
    memset(_ebuf->getMem(), 0, _ebuf->getMappingSize());
    // clear rb for polling
    memset(_dbuf->getMem(), 0, _dbuf->getMappingSize());
    
    _eb = (uint64_t *)_ebuf->getMem();
    _db = (struct MicrosliceDescriptor *)_dbuf->getMem();
    
    _dbentries = _dbuf->getMaxRBEntries();
    
    // Enable desciptor buffers and dma engine
    _ch->setEnableEB(1);
    _ch->setEnableRB(1);
    _ch->setDMAConfig( _ch->getDMAConfig() | 0x01 );
 
    return 0;
  }

  std::string _get_buffer_info(rorcfs_buffer* buf) {
    std::stringstream ss;
    ss << "start address = " << buf->getMem() <<", "
       << "physical size = " << (buf->getPhysicalSize() >> 20) << " MByte, "
       << "mapping size = "   << (buf->getMappingSize() >> 20) << " MByte, "
       << std::endl
       << "  end address = "    << (void*)((uint8_t *)buf->getMem() + buf->getPhysicalSize()) << ", "
       << "num SG entries = " << buf->getnSGEntries() << ", "
       << "max SG entries = " << buf->getMaxRBEntries();
    return ss.str();
  }

};
} // namespace flib

#pragma GCC diagnostic pop

#endif // FLIB_LINK_HPP
