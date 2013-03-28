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
  uint32_t flags; // unused, filled with 0
  uint32_t mc_size; // 16 bit words
  uint64_t dummy; // Pad to next 128 Bit entry, filled with 0 by HW
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

void dump_mc(uint64_t *eb, 
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
// poll for event outside of handler
int process_mc(uint64_t *eb, rb_entry *rb, unsigned int nr) {
  //DEBUG
  //printf("*** Process MC %d\n", nr);
  int error = 0;
  if( rb[nr].length == 0 ) {
    printf("ERROR: no MC available, rb.length = 0");
    error++;
    return error;
  }
  uint64_t mc_size = rb[nr].mc_size; // size is in 16 bit words
  uint64_t* mc_word = eb + rb[nr].offset/8; // offset is in bytes
  // hdr words
  uint64_t hdr0 = mc_word[0];
  uint64_t hdr1 = mc_word[1];
  unsigned int mch_hdrrev = (hdr0 >> 56) & 0xff;
  unsigned int mch_sysid =  (hdr0 >> 48) & 0xff;
  unsigned int mch_flags =  (hdr0 >> 32) & 0xffff;
  unsigned int mch_size =   hdr0 & 0xffffffff;
  unsigned int mch_rsvd = (hdr1 >> 48) & 0xffff;
  uint64_t mch_mc_nr = hdr1 & 0xffffffffffff;

  //printf("MC header :\n hdrrev 0x%02x, sysid 0x%02x, flags 0x%04x size 0x%08x\n rsvd 0x%04x mc_nr 0x%012lx\n",
  //         mch_hdrrev, mch_sysid, mch_flags, mch_size, mch_rsvd, mch_mc_nr);

  if( mch_hdrrev != MCH_HDRREV || mch_sysid != MCH_SYSID || mch_flags != MCH_FLAGS || mch_size != MCH_SIZE || mch_rsvd != MCH_RSVD ) {
    printf("ERROR: header wrong\n");
    error++;
  }
  
  uint8_t cneth_msg_cnt_save = 0;

  uint16_t* cnet_word = (uint16_t*) &(mc_word[2]);

//  for (int i = 0; i < whatever; i++) {
//    int j = (i & ~3) | (3 - (i & 3));
//    check(cnet_word[j]);
//  }

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
      // DEBUG
      //printf("Word %02d+%02d: %04x\n", w, i, cnet_word[w+i]);
    }
    // last word
    uint16_t cnet_msg_nr = cnet_word[w+i];
    //DEBUG
    //printf("Word %02d+%02d: msg_nr %04x\n", w, i, cnet_msg_nr);
    if( (cnet_msg_nr & 0xff) != ((cneth_msg_cnt+1) & 0xff)) { 
      printf("ERROR: wrong message number cnet_msg_nr 0x%02x cneth_msg_cnt 0x%02x\n",
             cnet_msg_nr, cneth_msg_cnt+1);
      error++;
    }
    w = w+i+(4-(w+i)%4); //set start index for next cnet message
}

  return error;
}

//int handle_channel_data( 
//                        struct rorcfs_event_descriptor *reportbuffer,
//                        unsigned int *eventbuffer,
//                        struct rorcfs_dma_channel *ch,
//                        struct ch_stats *stats,
//                        unsigned long rbsize,
//                        unsigned long maxrbentries,
//                        unsigned long max_events)
//{
//  unsigned long events_per_iteration = 0;
//  int events_processed = 0;
//  unsigned long eboffset = 0, rboffset = 0;
//  unsigned long starting_index, entrysize;
// 
// 
//  if( reportbuffer[stats->index].length!=0 ) { // new event received
// 
//    starting_index = stats->index;
// 
//    printf("handle_channel_data\n");
//    while( reportbuffer[stats->index].length!=0 && events_per_iteration < max_events) {
//      events_processed++;
// 
//      //stats->bytes_received += reportbuffer[stats->index].length;
//      stats->bytes_received += 
//        (reportbuffer[stats->index].size<<2);
//      //printf("event sizes: %d %d\n", reportbuffer[stats->index].length, (reportbuffer[stats->index].size<<2));
//      //dump_rb(rb, stats->index);
// 
//      
//      // set new EBOffset
//      eboffset = reportbuffer[stats->index].offset;
// 
//      // increment reportbuffer offset
//      rboffset = ((stats->index)*
//                  sizeof(struct rorcfs_event_descriptor)) % rbsize;
// 
//      // wrap RB pointer if necessary
//      if( stats->index < maxrbentries-1 ) 
//        stats->index++;
//      else
//        stats->index=0;
//      stats->n_events++;
//      events_per_iteration++;
//    }
// 
//    printf("processing events %ld..%ld (%ld)\n", starting_index, stats->index, events_per_iteration);
// 
//    // clear processed reportbuffer entries
//    entrysize = sizeof(struct rorcfs_event_descriptor);
//    //printf("clearing RB: start: %ld entries: %ld, %ldb each\n",
//    //	entrysize*starting_index, events_per_iteration, entrysize);
// 
//    memset(&reportbuffer[starting_index], 0, 
//           events_per_iteration*entrysize);
// 
//    ch->setEBOffset(eboffset);
//    ch->setRBOffset(rboffset);
//  }
// 
//  return events_processed;
//}


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

  uint64_t *eb[CHANNELS];
  struct rb_entry *rb[CHANNELS];

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
    eb[i] = (uint64_t *)ebuf[i]->getMem();
    // clear for debugging
    memset(eb[i], 0, ebuf[i]->getMappingSize());

    // get report buffer and clear for polling
    rb[i] = (struct rb_entry *)rbuf[i]->getMem();
    memset(rb[i], 0, rbuf[i]->getMappingSize());
    printf("pRBUF=%p, MappingSize=%ld\n", (void *)rbuf[i]->getMem(), rbuf[i]->getMappingSize() );
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
     // printf("RB raw\n");
     // dump_raw((uint64_t*)rb[i], 16);
     // printf("EB raw\n");
     // dump_raw(eb[i], 64);
//      for (int j = 1; j < 2; j++) {
//        printf("***MC: %d\n", j);
//        dump_mc(eb[i], rb[i], j);
//      }
      int error_cnt = 0;
      int mc_limit = 1000;
      for (int j = 1; j < mc_limit; j++) {
	int error = process_mc(eb[i], rb[i], j);
	error_cnt += error;
        if(error){
          dump_mc(eb[i], rb[i], j);	     
	}
      }
      printf("MCs analysed %d\nTotal errors: %d\n", mc_limit, error_cnt);
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
