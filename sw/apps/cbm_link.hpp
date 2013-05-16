#ifndef CBM_LINK_HPP
#define CBM_LINK_HPP

// EventBuffer size in bytes
const unsigned long EBUFSIZE_z = (((unsigned long)1) << 22);
// ReportBuffer size in bytes
const unsigned long RBUFSIZE = (((unsigned long)1) << 20);

struct __attribute__ ((__packed__)) rb_entry {
    uint64_t offset; // bytes
    uint64_t length; // bytes
    uint32_t flags; // unused, filled with 0
    uint32_t mc_size; // 16 bit words
    uint64_t dummy; // Pad to next 128 Bit entry, filled with 0 by HW
};

struct mc_desc {
    uint64_t nr;
    uint64_t* addr;
    uint32_t size; // bytes
    uint64_t length; // bytes
    uint64_t* rbaddr;
};

class cbm_link {
  
    rorcfs_buffer* _ebuf;
    rorcfs_buffer* _rbuf;
    rorcfs_dma_channel* _ch;
    unsigned int _channel;
  
    unsigned int _index;
    unsigned int _last_index;
    unsigned int _mc_nr;
    unsigned int _wrap;
  
    uint64_t* _eb;
    struct rb_entry* _rb;
  
    unsigned long _rbsize;
    unsigned long _rbentries;

public:
  
    struct mc_desc mc;

    cbm_link() : _ebuf(NULL), _rbuf(NULL), _ch(NULL), _index(0), _last_index(0),
                 _mc_nr(0), _wrap(0) { }

    ~cbm_link() {
        if(_ch)
            delete _ch;
        if(_ebuf)
            delete _ebuf;
        if(_rbuf)
            delete _rbuf;
    }
  

    int init(unsigned int channel,
             rorcfs_device* dev,
             rorcfs_bar* bar) {
    
        _channel = channel;
    
        // create new DMA event buffer
        _ebuf = new rorcfs_buffer();			
        if (_ebuf->allocate(dev, EBUFSIZE_z, 2*_channel, 
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

        //TODO: this is max payload size!!!
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
    
        return 0;
    };

    int deinit() {
        if(_ebuf->deallocate() != 0) {
            perror("ERROR: ebuf->deallocate");
            return -1;
        }
        if(_rbuf->deallocate() != 0) {
            perror("ERROR: rbuf->deallocate");
            return -1;
        }

        delete _ebuf;
        _ebuf = NULL;
        delete _rbuf;
        _rbuf = NULL;
        delete _ch;
        _ch = NULL;
    
        return 0;
    }

    int enable() {
        _ch->setEnableEB(1);
        _ch->setEnableRB(1);
        _ch->setDMAConfig( _ch->getDMAConfig() | 0x01 );
        return 0; 
    }

    int disable() {
        // disable DMA Engine
        _ch->setEnableEB(0);
        // wait for pending transfers to complete (dma_busy->0)
        while( _ch->getDMABusy() )
            usleep(100); 
        // disable RBDM
        _ch->setEnableRB(0);
        // reset DFIFO, disable DMA PKT
        _ch->setDMAConfig(0X00000002);
        return 0;
    }

    mc_desc* get_mc() {
        if(_rb[_index].length != 0) {
            mc.nr = _mc_nr;
            mc.addr = _eb + _rb[_index].offset/8;
            mc.size = _rb[_index].mc_size;
            mc.length = _rb[_index].length; // for checks only
            mc.rbaddr = (uint64_t *)&_rb[_index];

            // calculate next rb index
            _last_index = _index;
            if( _index < _rbentries-1 ) 
                _index++;
            else {
                _wrap++;
                _index = 0;
            }
            _mc_nr++;    

            return &mc;
        }
        else
#ifdef DEBUG  
            printf("MC %ld not available", _mc_nr);
#endif
        return NULL;
    }

    int ack_mc() {
        // TODO: EB pointers are set to begin of acknoledged entry, pointers are one entry delayed
        // to calculete end wrapping logic is required
        uint64_t eb_offset = _rb[_last_index].offset;
        uint64_t rb_offset = _last_index*sizeof(struct rb_entry) % _rbsize;

        // clear report buffer entry befor setting new pointers
        memset(&_rb[_last_index], 0, sizeof(struct rb_entry));
    
        // set pointers in HW
        _ch->setEBOffset(eb_offset);
        _ch->setRBOffset(rb_offset);

#ifdef DEBUG  
        printf("index %d EB offset set: %ld, get: %ld\n",
               _last_index, eb_offset, _ch->getEBOffset());
        printf("index %d RB offset set: %ld, get: %ld, wrap %d\n",
               _last_index, rb_offset, _ch->getRBOffset(), _wrap);
#endif
    
        return 0;
    }
  
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
