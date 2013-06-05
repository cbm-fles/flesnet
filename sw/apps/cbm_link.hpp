#ifndef CBM_LINK_HPP
#define CBM_LINK_HPP

#include "librorc.h"

const size_t log_ebufsize =  22;
const size_t log_rbufsize =  20;

// EventBuffer size in bytes
const unsigned long EBUFSIZE = (((unsigned long)1) << log_ebufsize);
// ReportBuffer size in bytes
const unsigned long RBUFSIZE = (((unsigned long)1) << log_rbufsize);

// has to be 256 Bit, this is hard coded in hw
struct __attribute__ ((__packed__)) rb_entry {
  uint8_t   hdr_id;  // "Header format identifier" DD
  uint8_t   hdr_ver; // "Header format version"    01
  uint16_t  eq_id;   // "Equipment identifier"     F001
  uint16_t  flags;   // "Status and error flags"   0s
  uint8_t   sys_id;  // "Subsystem identifier"     AA
  uint8_t   sys_ver; // "Subsystem format version" AA
  uint64_t  idx;     // "Microslice index"
  uint32_t  crc;     // "CRC32 checksum"
  uint32_t  size;    // "Size in 16 bit words"
  uint64_t  offset;  // "Ofsset in event buffer"
};

struct mc_desc {
    uint64_t nr;
    volatile uint64_t* addr;
    uint32_t size; // bytes
    volatile uint64_t* rbaddr;
};

class cbm_link {
  
    rorcfs_buffer* _ebuf;
    rorcfs_buffer* _rbuf;
    rorcfs_dma_channel* _ch;
    unsigned int _channel;
  
    unsigned int _index;
    unsigned int _last_index;
    unsigned int _last_acked;
    unsigned int _mc_nr;
    unsigned int _wrap;
  
    volatile uint64_t* _eb;
    volatile struct rb_entry* _rb;
  
    unsigned long _rbsize;
    unsigned long _rbentries;

public:
  
    //    struct mc_desc mc;

    cbm_link() : _ebuf(NULL), _rbuf(NULL), _ch(NULL), _index(0), _last_index(0),
                 _last_acked(0), _mc_nr(0), _wrap(0) { }

    ~cbm_link() {
      if(_ch) {
        // disable DMA Engine
        _ch->setEnableEB(0);
        // wait for pending transfers to complete (dma_busy->0)
        //while( _ch->getDMABusy() ) TODO
         usleep(100);
        // disable RBDM
        _ch->setEnableRB(0);
        // disable DMA PKT
        //TODO: if resetting DFIFO by setting 0x2 restart is impossible
        _ch->setDMAConfig(0X00000000);
        printf("DMA disabled\n");
      }
      if(_ebuf){
        if(_ebuf->deallocate() != 0) {
          std::cout << "ERROR: ebuf->deallocate" << std::endl;
          throw 1;
        }
        delete _ebuf;
        _ebuf = NULL;
      }
      if(_rbuf){
        if(_rbuf->deallocate() != 0) {
          std::cout << "ERROR: rbuf->deallocate" << std::endl;
          throw 1;
        }
        delete _rbuf;
        _rbuf = NULL;
      }
      if(_ch) {
        delete _ch;
        _ch = NULL;
      }
    }  

    int init(unsigned int channel,
             rorcfs_device* dev,
             rorcfs_bar* bar) {
    
        _channel = channel;
    
        // create new DMA event buffer
        _ebuf = new rorcfs_buffer();			
        if (_ebuf->allocate(dev, EBUFSIZE, 2*_channel, 
                            1, RORCFS_DMA_FROM_DEVICE)!=0) {
            if (errno == EEXIST) {
                printf("INFO: Buffer ebuf %d already exists, trying to connect ...\n", 2*_channel);
                if ( _ebuf->connect(dev, 2*_channel) != 0 ) {
                    perror("ERROR: ebuf->connect");
                    return -1;
                }
            } else {
                perror("ERROR: ebuf->allocate");
                return -1;
            }
        }
        printf("INFO: pEBUF=%p, PhysicalSize=%ld MBytes, MappingSize=%ld MBytes,\n"
               "    EndAddr=%p, nSGEntries=%ld\n", 
               (void *)_ebuf->getMem(), _ebuf->getPhysicalSize() >> 20,
               _ebuf->getMappingSize() >> 20, 
               (uint8_t *)_ebuf->getMem() + _ebuf->getPhysicalSize(), 
               _ebuf->getnSGEntries());

        // create new DMA report buffer
        _rbuf = new rorcfs_buffer();
        if (_rbuf->allocate(dev, RBUFSIZE, 2*_channel+1, 
                            1, RORCFS_DMA_FROM_DEVICE)!=0) {
            if (errno == EEXIST) {
                printf("INFO: Buffer rbuf %d already exists, trying to connect ...\n",
                       2*_channel+1);
                if (_rbuf->connect(dev, 2*_channel+1) != 0) {
                    perror("ERROR: rbuf->connect");
                    return -1;
                }
            } else {
                perror("ERROR: rbuf->allocate");
                return -1;
            }
        }
        printf("INFO: pRBUF=%p, PhysicalSize=%ld MBytes, MappingSize=%ld MBytes,\n"
               "    EndAddr=%p, nSGEntries=%ld, MaxRBEntries=%ld\n", 
               (void *)_rbuf->getMem(), _rbuf->getPhysicalSize() >> 20,
               _rbuf->getMappingSize() >> 20, 
               (uint8_t *)_rbuf->getMem() + _rbuf->getPhysicalSize(), 
               _rbuf->getnSGEntries(), _rbuf->getMaxRBEntries() );

        // create DMA channel
        _ch = new rorcfs_dma_channel();
        // bind channel to BAR1, channel offset 0
        _ch->init(bar, (_channel+1)*RORC_CHANNEL_OFFSET);

        // prepare EventBufferDescriptorManager
        // and ReportBufferDescriptorManage
        // with scatter-gather list
        if( _ch->prepareEB(_ebuf) < 0 ) {
            perror("prepareEB()");
            return -1;
        }
        if( _ch->prepareRB(_rbuf) < 0 ) {
            perror("prepareRB()");
            return -1;
        }

        if( _ch->configureChannel(_ebuf, _rbuf, 128) < 0) {
            perror("configureChannel()");
            return -1;
        }

        // clear eb for debugging
        memset(_ebuf->getMem(), 0, _ebuf->getMappingSize());
        // clear rb for polling
        memset(_rbuf->getMem(), 0, _rbuf->getMappingSize());

        _eb = (uint64_t *)_ebuf->getMem();
        _rb = (struct rb_entry *)_rbuf->getMem();

        _rbsize = _rbuf->getPhysicalSize();
        _rbentries = _rbuf->getMaxRBEntries();

        // Enable desciptor buffers and dma engine
        _ch->setEnableEB(1);
        _ch->setEnableRB(1);
        _ch->setDMAConfig( _ch->getDMAConfig() | 0x01 );
    
        return 0;
    };

    int enable_TODO() {
      // hold data source here
        return 0; 
    }

    int disable_TODO() {
        return 0;
    }
  
    std::pair<mc_desc, bool> get_mc() {
        struct mc_desc mc;
        if(_rb[_index].idx > _mc_nr) { // mc_nr counts from 1 in HW
          _mc_nr = _rb[_index].idx;
          mc.nr = _mc_nr;
          mc.addr = _eb + _rb[_index].offset/sizeof(uint64_t);
          mc.size = _rb[_index].size << 1; // size in rb is in 16 bit words
          mc.rbaddr = (uint64_t *)&_rb[_index];

          // calculate next rb index
          _last_index = _index;
          if( _index < _rbentries-1 ) 
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
    uint64_t eb_offset = _rb[_last_index].offset;
    // each rbenty is 32 bytes, this is hard coded in HW
    uint64_t rb_offset = _last_index*sizeof(struct rb_entry) % _rbsize;

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
  
    // TODO: Add funtions to set channel properties like data source, link reset, activate busys 


    rorcfs_buffer* ebuf() const {
        return _ebuf;
    }

    rorcfs_buffer* rbuf() const {
        return _rbuf;
    }

    rorcfs_dma_channel* get_ch() const {
        return _ch;
    }
};

#endif // CBM_LINK_HPP
