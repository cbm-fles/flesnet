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


struct ch_stats {
  unsigned long n_events;
  unsigned long bytes_received;
  unsigned long index;
  unsigned long error_count;
  int last_id;
  unsigned int channel;
};

struct rb_entry {
  uint64_t offset; // bytes
  uint64_t length; // bytes
  uint32_t calc_event_size; // 16 bit words
  uint32_t reported_event_size; // 16 bit words
  uint64_t dummy; // Pad to next 128 Bit entry, filled with 0 by HW
};

void dump_report(struct rb_entry *rb, unsigned int nr)
{
  printf("Report #%d addr=%p :\n offset=%lu, length=%lu, calc_size=%u reported_size=%u Bytes\n",
         nr,
         &rb[nr],
         rb[nr].offset,
	 rb[nr].length,
	 rb[nr].calc_event_size<<1,
	 rb[nr].reported_event_size<<1);
}

void dump_mc(uint64_t *eb, 
	     rb_entry *rb,
	     unsigned int nr)
{
  dump_report(rb, nr);
  unsigned int i = 0;
  printf("Event	 #%d\n", nr);
  // length and offset is in bytes, adressing is per 8 Bytes 
  uint64_t offset = rb[nr].offset/8;
  for (i = 0; i < rb[nr].length/8; i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i, (eb+i+offset), *(eb+i+offset), *(eb+i+1+offset));
  }
}

void dump_mc_32(uint32_t *eb, 
	     rb_entry *rb,
	     unsigned int nr)
{
  dump_report(rb, nr);
  unsigned int i = 0;
  printf("Event  #%d\n", nr);
  // length and offset is in bytes, adressing is per 4 Bytes 
  uint64_t offset = rb[nr].offset/4;
  for (i = 0; i < rb[nr].length/4; i+=4) {
    printf("%4d addr=%p:    %08x %08x %08x %08x\n",
	   i, (eb+i+offset), *(eb+i+1+offset), *(eb+i+offset), *(eb+i+3+offset), *(eb+i+2+offset));
  }
}

void dump_eb_raw(uint64_t *eb, unsigned int size)
{
  unsigned int i = 0;
  for (i = 0; i < size; i+=2) {
    printf("%d: %016lx %016lx\n", i, *(eb+i), *(eb+i+1)); 
  }
}

void dump_eb_raw_32( uint32_t *eb, unsigned int size)
{
  unsigned int i = 0;
  for (i = 0; i < size; i+=4) {
    printf("%d: %08x %08x %08x %08x\n", i, *(eb+i+1), *(eb+i), *(eb+i+3), *(eb+i+2)); 
  }
}

void dump_rb_raw( uint32_t *rb, unsigned int size)
{
  unsigned int i = 0;
  for (i = 0; i < size; i+=8) {
    printf("%d: %08x %08x %08x %08x %08x %08x %08x %08x\n", i, *(rb+i+1), *(rb+i), *(rb+i+3), *(rb+i+2), *(rb+i+5), *(rb+i+4), *(rb+i+7), *(rb+i+6));
  }
}


#define HDRREV 0x01
#define SYSID 0xaa
#define FLAGS 0xffff
#define SIZE 0x12345678
#define RSVD 0xabcd

//--------------------------------
//poll for event outside of handler
int process_mc(uint64_t *eb, rb_entry *rb, unsigned int nr) {
  if( rb[nr].length == 0 ) {
    printf("ERROR: no MC avialable, rb.length = 0");
    return 0;
  }
  // offset is in bytes, adressing is pre 8 Bytes
  uint64_t offset = rb[nr].offset/8;
  uint64_t calc_size = rb[nr].calc_event_size/4; // size is in 16 bit words
  // hdr word 0
  unsigned int word = 0;
  unsigned int hdrrev = (*(eb+offset+word)>>56) & 0xff;
  unsigned int sysid =  (*(eb+offset+word)>>48) & 0xff;
  unsigned int flags =  (*(eb+offset+word)>>32) & 0xffff;
  unsigned int size =   (*(eb+offset+word)) & 0xffffffff;
  // hdr word 1
  word++;
  unsigned int rsvd = (*(eb+offset+word)>>48) & 0xffff;
  uint64_t mc_nr = (*(eb+offset+word)) & 0xffffffffffff;

  printf("MC header :\n hdrrev %02x, sysid %02x, flags %04x size %08x\n rsvd %04x mc_nr %012lx\n",
         hdrrev, sysid, flags, size, rsvd, mc_nr);

  if( hdrrev != HDRREV || sysid != SYSID || flags != FLAGS || size != SIZE || rsvd != RSVD ) {
    printf("ERROR: header wrong\n");
    return 0;
  }
  
  word++;
  // check cnet messages
  uint8_t cneth_msg_cnt = ((*(eb+offset+word)>>56) & 0xff)-1; // initialize to msg_cnt-1 for first check
  uint8_t cneth_msg_cnt_befor = 0;
  uint8_t cneth_wrd_cnt = 0;
  uint16_t cnet_src_addr = 0;

  while(word < calc_size) {
    // TODO: maybe save message count for next mc
    // cnet header
    cneth_msg_cnt_befor = cneth_msg_cnt;
    cneth_msg_cnt = (*(eb+offset+word)>>56) & 0xff;
      if ((cneth_msg_cnt & 0xff) != (cneth_msg_cnt_befor & 0xff)+1 ) { // check message count
        printf("ERROR: message count %04x %04x\n", cneth_msg_cnt, cneth_msg_cnt_befor);
      }
    cneth_wrd_cnt = (*(eb+offset+word)>>48) & 0xff;
    cnet_src_addr = (*(eb+offset+word)>>32) & 0xffff;
    printf("Cnet hdr: msg_cnt %02x wrd_cnt %02x src_addr %04x\n", cneth_msg_cnt, cneth_wrd_cnt, cnet_src_addr);

    // first cnet word
    uint16_t cnet_word = 0;
    uint16_t cnet_word_befor = 0;
    unsigned i = 2;
    cnet_word = (*(eb+offset+word+i/4)>>(48-i*16)) & 0xffff;
    printf("Word %d: %04x\n", i, cnet_word);

    // all other cnet words
    for( unsigned int i = 3; i <= cneth_wrd_cnt; i++) {
      cnet_word_befor = cnet_word;
      cnet_word = (*(eb+offset+word+i/4)>>(48-i*16)) & 0xffff;
      if ((cnet_word & 0xff) != (cnet_word_befor & 0xff)+1 ) { // check word ramp
        printf("ERROR: wrong cnet word %04x %04x\n", cnet_word, cnet_word_befor);
      }
      printf("Word %d: %04x\n", i, cnet_word);
    }
    i = cneth_wrd_cnt+1;
    uint16_t cnet_msg_nr = (*(eb+offset+word+i/4)>>(48-i*16)) & 0xffff;
    printf("msg_nr %d: %04x\n", i, cnet_msg_nr);
    if( (cnet_msg_nr & 0xff) != cneth_msg_cnt+1) { 
      printf("ERROR: wrong message number\n");
    }
    word = word+1+i/4;
    printf("word: %d\n", word);
  }

  return 1;
}

int handle_channel_data( 
                        struct rorcfs_event_descriptor *reportbuffer,
                        unsigned int *eventbuffer,
                        struct rorcfs_dma_channel *ch,
                        struct ch_stats *stats,
                        unsigned long rbsize,
                        unsigned long maxrbentries,
                        unsigned long max_events)
{
  unsigned long events_per_iteration = 0;
  int events_processed = 0;
  unsigned long eboffset = 0, rboffset = 0;
  unsigned long starting_index, entrysize;


  if( reportbuffer[stats->index].length!=0 ) { // new event received

    starting_index = stats->index;

    printf("handle_channel_data\n");
    while( reportbuffer[stats->index].length!=0 && events_per_iteration < max_events) {
      events_processed++;

      //stats->bytes_received += reportbuffer[stats->index].length;
      stats->bytes_received += 
        (reportbuffer[stats->index].reported_event_size<<2);
      //printf("event sizes: %d %d\n", reportbuffer[stats->index].length, (reportbuffer[stats->index].reported_event_size<<2));
      //dump_rb(rb_struct, stats->index);

      
      // set new EBOffset
      eboffset = reportbuffer[stats->index].offset;

      // increment reportbuffer offset
      rboffset = ((stats->index)*
                  sizeof(struct rorcfs_event_descriptor)) % rbsize;

      // wrap RB pointer if necessary
      if( stats->index < maxrbentries-1 ) 
        stats->index++;
      else
        stats->index=0;
      stats->n_events++;
      events_per_iteration++;
    }

    printf("processing events %ld..%ld (%ld)\n", starting_index, stats->index, events_per_iteration);

    // clear processed reportbuffer entries
    entrysize = sizeof(struct rorcfs_event_descriptor);
    //printf("clearing RB: start: %ld entries: %ld, %ldb each\n",
    //	entrysize*starting_index, events_per_iteration, entrysize);

    memset(&reportbuffer[starting_index], 0, 
           events_per_iteration*entrysize);

    ch->setEBOffset(eboffset);
    ch->setRBOffset(rboffset);
  }

  return events_processed;
}


// EventBuffer size in bytes
#define EBUFSIZE (((unsigned long)1) << 30)
// ReportBuffer size in bytes
#define RBUFSIZE (((unsigned long)1) << 28)
// number of bytes to transfer
#define N_BYTES_TRANSFER (((unsigned long)1) << 10)

#define CHANNELS 1


int main()
{
  int result = 0;
  unsigned long i;
  rorcfs_device *dev = NULL;
  rorcfs_bar *bar1 = NULL;
  rorcfs_buffer *ebuf[CHANNELS];
  rorcfs_buffer *rbuf[CHANNELS];
  rorcfs_dma_channel *ch[CHANNELS];

  struct ch_stats *chstats[CHANNELS];

  uint32_t *rb_raw[CHANNELS];
  uint32_t *eb_raw[CHANNELS];
  uint64_t *eb_64[CHANNELS];
  struct rb_entry *rb_struct[CHANNELS];

  for (i=0;i<CHANNELS;i++) {
    ebuf[i]=NULL;
    rbuf[i]=NULL;
    ch[i]=NULL;
    chstats[i]=NULL;
  }

  // create new device class
  dev = new rorcfs_device();	
  if (dev->init(0) == -1) {
    cout << "failed to initialize device 0" << endl;
    goto out;
  }

  printf("Bus %x, Slot %x, Func %x\n", dev->getBus(),
         dev->getSlot(),dev->getFunc());

  // bind to BAR1
  bar1 = new rorcfs_bar(dev, 1);
  if ( bar1->init() == -1 ) {
    cout<<"BAR1 init failed\n";
    goto out;
  }

  printf("FirmwareDate: %08x\n", bar1->get(RORC_REG_FIRMWARE_DATE));

  for(i=0;i<CHANNELS;i++) {

    // create new DMA event buffer
    ebuf[i] = new rorcfs_buffer();			
    if ( ebuf[i]->allocate(dev, EBUFSIZE, 2*i, 
                           1, RORCFS_DMA_FROM_DEVICE)!=0 ) {
      if ( errno == EEXIST ) {
        if ( ebuf[i]->connect(dev, 2*i) != 0 ) {
          perror("ERROR: ebuf->connect");
          goto out;
        }
      } else {
        perror("ERROR: ebuf->allocate");
        goto out;
      }
    }

    // create new DMA report buffer
    rbuf[i] = new rorcfs_buffer();;
    if ( rbuf[i]->allocate(dev, RBUFSIZE, 2*i+1, 
                           1, RORCFS_DMA_FROM_DEVICE)!=0 ) {
      if ( errno == EEXIST ) {
        //printf("INFO: Buffer already exists, trying to connect...\n");
        if ( rbuf[i]->connect(dev, 2*i+1) != 0 ) {
          perror("ERROR: rbuf->connect");
          goto out;
        }
      } else {
        perror("ERROR: rbuf->allocate");
        goto out;
      }
    }

    chstats[i] = (struct ch_stats*)malloc(sizeof(struct ch_stats));
    if ( chstats[i]==NULL ){
      perror("alloc chstats");
      result=-1;
      goto out;
    }

    memset(chstats[i], 0, sizeof(struct ch_stats));

    chstats[i]->index = 0;
    chstats[i]->last_id = -1;
    chstats[i]->channel = (unsigned int)i;


    // create DMA channel
    ch[i] = new rorcfs_dma_channel();

    // bind channel to BAR1, channel offset 0
    ch[i]->init(bar1, (i+1)*RORC_CHANNEL_OFFSET);

    // prepare EventBufferDescriptorManager
    // with scatter-gather list
    result = ch[i]->prepareEB( ebuf[i] );
    if (result < 0) {
      perror("prepareEB()");
      result = -1;
      goto out;
    }

    // prepare ReportBufferDescriptorManager
    // with scatter-gather list
    result = ch[i]->prepareRB( rbuf[i] );
    if (result < 0) {
      perror("prepareRB()");
      result = -1;
      goto out;
    }

    result = ch[i]->configureChannel(ebuf[i], rbuf[i], 128);
    if (result < 0) {
      perror("configureChannel()");
      result = -1;
      goto out;
    }
    
    // get event buffer
    eb_raw[i] = (uint32_t *)ebuf[i]->getMem();
    // clear for debugging
    memset(eb_raw[i], 0, ebuf[i]->getMappingSize());

    // get report buffer and clear for polling
    rb_raw[i] = (uint32_t *)rbuf[i]->getMem();
    memset(rb_raw[i], 0, rbuf[i]->getMappingSize());
    printf("pRBUF=%p, MappingSize=%ld\n", rbuf[i]->getMem(), rbuf[i]->getMappingSize() );
    rb_struct[i] = (struct rb_entry *)rbuf[i]->getMem();

    eb_64[i] = (uint64_t *)ebuf[i]->getMem();
  }

  for(i=0;i<CHANNELS;i++) {
    ch[i]->setEnableEB(1);
    ch[i]->setEnableRB(1);
  }
	
  //  bar1->gettime(&start_time, 0);

  for(i=0;i<CHANNELS;i++) {
    // enable DMA channel
    ch[i]->setDMAConfig( ch[i]->getDMAConfig() | 0x01 );
  }

  i = 0;

  sleep(1);

  while( 1 ) {

    for(i=0;i<CHANNELS;i++) {

      printf("RB raw\n");
      dump_rb_raw(rb_raw[i], 32);
      printf("EB raw\n");
      dump_eb_raw(eb_64[i], 64);
      printf("evnet\n");
      for (int j = 0; j < 20; j++) {
        printf("*** #%d dump mc\n", j);
        dump_mc(eb_64[i], rb_struct[i], j);
        printf("*** #%d process mc\n", j);
        process_mc(eb_64[i], rb_struct[i], j);
      }
      
      goto out;
    }
  }

  for(i=0;i<CHANNELS;i++) {
    // disable DMA Engine
    ch[i]->setEnableEB(0);
  }
	
  for(i=0;i<CHANNELS;i++) {

    // wait for pending transfers to complete (dma_busy->0)
    while( ch[i]->getDMABusy() )
      usleep(100);

    // disable RBDM
    ch[i]->setEnableRB(0);

    // reset DFIFO, disable DMA PKT
    ch[i]->setDMAConfig(0X00000002);

    if (chstats[i]) {
      free(chstats[i]);
      chstats[i] = NULL;
    }
    if (ch[i]) {
      delete ch[i];
      ch[i] = NULL;
    }
    if (ebuf[i]) {
      delete ebuf[i];
      ebuf[i] = NULL;
    }
    if (rbuf[i]) {
      delete rbuf[i];
      rbuf[i] = NULL;
    }
  }

 out:

  for(i=0;i<CHANNELS;i++) {
    if (chstats[i])
      free(chstats[i]);
    if (ch[i])
      delete ch[i];
    if (ebuf[i])
      delete ebuf[i];
    if (rbuf[i])
      delete rbuf[i];
  }

  if (bar1)
    delete bar1;
  if (dev)
    delete dev;

  return result;
}
