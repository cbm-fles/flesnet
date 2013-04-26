#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

#include "librorc.h"

using namespace std;

// EventBuffer size in bytes
#define EBUFSIZE_z (((unsigned long)1) << 22)
// ReportBuffer size in bytes
#define RBUFSIZE (((unsigned long)1) << 20)
// number of bytes to transfer
#define N_BYTES_TRANSFER (((unsigned long)1) << 10)

#define CHANNELS 1

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

  cbm_link() : _ebuf(NULL), _rbuf(NULL), _ch(NULL), _index(0), _last_index(0), _mc_nr(0), _wrap(0) { }

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
    if ( _ebuf->allocate(dev, EBUFSIZE_z, 2*_channel, 
                           1, RORCFS_DMA_FROM_DEVICE)!=0 ) {
      if ( errno == EEXIST ) {
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
    printf("INFO: pEBUF=%p, PhysicalSize=%ld MBytes, MappingSize=%ld MBytes,\n    EndAddr=%p, nSGEntries=%ld\n", 
           (void *)_ebuf->getMem(), _ebuf->getPhysicalSize() >> 20, _ebuf->getMappingSize() >> 20, 
           (uint8_t *)_ebuf->getMem() + _ebuf->getPhysicalSize(), 
           _ebuf->getnSGEntries());

    // create new DMA report buffer
    _rbuf = new rorcfs_buffer();
    if ( _rbuf->allocate(dev, RBUFSIZE, 2*_channel+1, 
                           1, RORCFS_DMA_FROM_DEVICE)!=0 ) {
      if ( errno == EEXIST ) {
        printf("INFO: Buffer rbuf %d already exists, trying to connect ...\n", 2*_channel+1);
        if ( _rbuf->connect(dev, 2*_channel+1) != 0 ) {
          perror("ERROR: rbuf->connect");
          return -1;
        }
      } else {
        perror("ERROR: rbuf->allocate");
        return -1;
      }
    }
    printf("INFO: pRBUF=%p, PhysicalSize=%ld MBytes, MappingSize=%ld MBytes,\n    EndAddr=%p, nSGEntries=%ld, MaxRBEntries=%ld\n", 
           (void *)_rbuf->getMem(), _rbuf->getPhysicalSize() >> 20, _rbuf->getMappingSize() >> 20, 
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
    printf("index %d EB offset set: %ld, get: %ld\n", _last_index, eb_offset, _ch->getEBOffset());
    printf("index %d RB offset set: %ld, get: %ld, wrap %d\n", _last_index, rb_offset, _ch->getRBOffset(), _wrap);
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


// DUMP functions
void dump_raw(uint64_t *buf, unsigned int size)
{
  unsigned int i = 0;
  for (i = 0; i < size; i+=2) {
    printf("%p: %016lx %016lx\n", (void *)(buf+i), *(buf+i+1), *(buf+i)); 
  }
}

void dump_report(struct rb_entry *rb, unsigned int nr)
{
  printf("Report #%d addr=%p :\n offset=%lu, length=%lu, mc_size=%u Bytes\n",
         nr,
         (void *)&rb[nr],
         rb[nr].offset,
	 rb[nr].length,
	 rb[nr].mc_size<<1);
}

void dump_mc(mc_desc* mc)
{
  printf("Report addr=%p :\n length=%lu, mc_size=%u Bytes\n",
         (void *)mc->rbaddr,
         mc->length,
         mc->size<<1);
  // length and offset is in bytes, adressing is per 8 Bytes 
  uint64_t* mc_word = mc->addr;
  printf("Event	 #%ld\n", mc->nr);
  for (unsigned int i = 0; i < ((mc->length)/8); i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i*8, (void *)&mc_word[i], mc_word[i+1], mc_word[i]);
  }
}

// Dumps mc correponding to entry nr in report buffer
// will not accout for wrapping, no range check
void dump_mc_raw(uint64_t *eb,
	     rb_entry *rb,
	     unsigned int nr)
{
  dump_report(rb, nr);
  printf("Event	 #%d\n", nr);
  // length and offset is in bytes, adressing is per 8 Bytes 
  uint64_t* mc_word = eb + rb[nr].offset/8;
  for (unsigned int i = 0; i < (rb[nr].length/8); i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i*8, (void *)&mc_word[i], mc_word[i+1], mc_word[i]);
  }
}

#define MCH_HDRREV 0x01
#define MCH_SYSID 0xaa
#define MCH_FLAGS 0xffff
#define MCH_SIZE 0x12345678
#define MCH_RSVD 0xabcd

//--------------------------------
int process_mc(mc_desc* mc) {
  
  int error = 0;
  uint64_t mc_nr = mc->nr;
  uint32_t mc_size = mc->size; // size is in 16 bit words
  uint64_t* mc_word = mc->addr;
  // hdr words
  uint64_t hdr0 = mc_word[0];
  uint64_t hdr1 = mc_word[1];
  unsigned int mch_hdrrev = (hdr0 >> 56) & 0xff;
  unsigned int mch_sysid =  (hdr0 >> 48) & 0xff;
  unsigned int mch_flags =  (hdr0 >> 32) & 0xffff;
  unsigned int mch_size =   hdr0 & 0xffffffff;
  unsigned int mch_rsvd = (hdr1 >> 48) & 0xffff;
  uint64_t mch_mc_nr = hdr1 & 0xffffffffffff;

#ifdef DEBUG
  printf("MC nr %ld, addr %p, size %d \n", mc_nr, (void *)mc_word, mc_size*2);
  //  printf("MC header :\n hdrrev 0x%02x, sysid 0x%02x, flags 0x%04x size 0x%08x\n rsvd 0x%04x mc_nr 0x%012lx\n",
  //         mch_hdrrev, mch_sysid, mch_flags, mch_size, mch_rsvd, mch_mc_nr);
#endif

  if( mch_hdrrev != MCH_HDRREV || mch_sysid != MCH_SYSID || mch_flags != MCH_FLAGS || mch_size != MCH_SIZE || mch_rsvd != MCH_RSVD ) {
    printf("ERROR: wrong MC header\n");
    printf("MC header :\n hdrrev 0x%02x, sysid 0x%02x, flags 0x%04x size 0x%08x\n rsvd 0x%04x mc_nr 0x%012lx\n", mch_hdrrev, mch_sysid, mch_flags, mch_size, mch_rsvd, mch_mc_nr);
    error++;
  }
  
  uint8_t cneth_msg_cnt_save = 0;

  uint16_t* cnet_word = (uint16_t*) &(mc_word[2]);

  // Check cnet messages
  unsigned int w = 0;
  while(w < mc_size-8) {
    // TODO: save message count for next mc
    // first and second 
    uint8_t cneth_wrd_cnt = cnet_word[w] & 0xff;
    uint8_t cneth_msg_cnt = (cnet_word[w] >> 8) & 0xff;
    uint16_t cnet_src_addr = cnet_word[w+1];
    //printf("Cnet hdr: w: %d msg_cnt 0x%02x wrd_cnt 0x%02x src_addr 0x%04x\n",
    //       w, cneth_msg_cnt, cneth_wrd_cnt, cnet_src_addr);
    w +=2;

    // check message count
    if (cneth_msg_cnt != ((cneth_msg_cnt_save+1) & 0xff) && w > 2) {
      printf("ERROR: wrong message count now: 0x%02x before+1: 0x%02x\n", cneth_msg_cnt, cneth_msg_cnt_save+1);
      error++;
    }
    cneth_msg_cnt_save = cneth_msg_cnt;
    
    uint16_t cnet_word_save = 0;

    // message ramp
    int i = 0;
    for (i = 0; i <= cneth_wrd_cnt-2; i++) {      
      if (((cnet_word[w+i] & 0xff) != (cnet_word_save & 0xff) + 1 && i > 0) || 
          (cnet_word[w] != 0xbc00 && i == 0) ) {
        printf("ERROR: wrong cnet word 0x%04x 0x%04x\n", cnet_word[w+i], cnet_word_save);
        error++;
        return error;
      }
      cnet_word_save = cnet_word[w+i];
#ifdef DEBUG_2
      printf("Word %02d+%02d: %04x\n", w, i, cnet_word[w+i]);
#endif
    }
    // last word
    uint16_t cnet_msg_nr = cnet_word[w+i];
#ifdef DEBUG_2
    printf("Word %02d+%02d: msg_nr %04x\n", w, i, cnet_msg_nr);
#endif
    if( (cnet_msg_nr & 0xff) != ((cneth_msg_cnt+1) & 0xff)) { 
      printf("ERROR: wrong message number cnet_msg_nr 0x%02x cneth_msg_cnt 0x%02x\n",
             cnet_msg_nr, cneth_msg_cnt+1);
      error++;
    }
    w = w+i+(4-(w+i)%4); //set start index for next cnet message
}
  
  return error;
}


rorcfs_device *dev = NULL;
rorcfs_bar *bar1 = NULL;
cbm_link* cbmLink[CHANNELS] = {NULL};

void fnExit (void)
{
  for(int i=0;i<CHANNELS;i++) {
    if (cbmLink[i]){
      //cbmLink[i]->disable();
      cbmLink[i]->deinit();
      delete cbmLink[i];
    }
  }
  if (bar1)
    delete bar1;
  if (dev)
    delete dev;  
  printf("Exiting\n");
}

int main(int argc, char *argv[])
{ 
  atexit(fnExit);
  
  int i = 0;
  uint64_t *eb[CHANNELS];
  struct rb_entry *rb[CHANNELS];
  rorcfs_dma_channel* chan[CHANNELS] = {NULL};

  if(argc != 2) {
    printf("Usage: %s <#mc>\n", argv[0]);
    return -1;
  }

  int mc_limit = atoi(argv[1]);

  // create new device class
  dev = new rorcfs_device();	
  if (dev->init(0) == -1) {
    cout << "failed to initialize device 0" << endl;
    return -1;
  }

  printf("Bus %x, Slot %x, Func %x\n", dev->getBus(),
         dev->getSlot(),dev->getFunc());

  // bind to BAR1
  bar1 = new rorcfs_bar(dev, 1);
  if ( bar1->init() == -1 ) {
    cout<<"BAR1 init failed\n";
    return -1;
  }

  printf("FirmwareDate: %08x\n", bar1->get(RORC_REG_FIRMWARE_DATE));

  for(i=0;i<CHANNELS;i++) {
    
    cbmLink[i] = new cbm_link();
    
    if( cbmLink[i]->init(i, dev, bar1) < 0) {
      perror("ERROR: cbmLink->init");
    }
  }
  
  for(i=0;i<CHANNELS;i++) {
    if (cbmLink[i]->enable() < 0) {
      perror("ERROR: cbmLink->enable");
    }
  }

  for(i=0;i<CHANNELS;i++) {
    eb[i] = (uint64_t *)cbmLink[i]->ebuf()->getMem();
    rb[i] = (struct rb_entry *)cbmLink[i]->rbuf()->getMem();
    chan[i] = cbmLink[i]->get_ch();
  }

  for(i=0;i<CHANNELS;i++) {
    // printf("RB raw\n");
    // dump_raw((uint64_t*)rb[i], 16);
    // printf("EB raw\n");
    // dump_raw(eb[i], 64);
    //      for (int j = 1; j < 2; j++) {
    //        printf("***MC: %d\n", j);
    //        dump_mc_raw(eb[i], rb[i], j);
    //      }
    int error_cnt = 0;
    for (int j = 0; j < mc_limit; j++) {

      // poll for mc
      mc_desc* mc = cbmLink[i]->get_mc();
      while( mc == NULL ) {
        printf("waiting\n");
        sleep(1);
        mc = cbmLink[i]->get_mc();
      }
//      int error = process_mc(mc);
//      error_cnt += error;
//      if(error){
//        dump_mc(mc);	     
//      }
      if(j % 1000000 == 0) {
	dump_mc(mc);	     
        //        dump_mc_raw(eb[i], rb[i], j);
      }

      cbmLink[i]->ack_mc();
      if((j & 0xFFFFF) == 0xFFFFF) {
        printf("%d analysed\n", j);
      }

    }
    printf("MCs analysed %d\nTotal errors: %d\n", mc_limit, error_cnt);
  }

  return 0;
}
