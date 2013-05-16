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

#include "cbm_link.hpp"

using namespace std;

const unsigned int CHANNELS = 1;



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
  //  printf("MC header :\n hdrrev 0x%02x, sysid 0x%02x, flags 0x%04x size 0x%08x\n"
  //         " rsvd 0x%04x mc_nr 0x%012lx\n",
  //         mch_hdrrev, mch_sysid, mch_flags, mch_size, mch_rsvd, mch_mc_nr);
#endif

  if( mch_hdrrev != MCH_HDRREV || mch_sysid != MCH_SYSID || mch_flags != MCH_FLAGS
      || mch_size != MCH_SIZE || mch_rsvd != MCH_RSVD ) {
    printf("ERROR: wrong MC header\n");
    printf("MC header :\n hdrrev 0x%02x, sysid 0x%02x, flags 0x%04x size 0x%08x\n"
           " rsvd 0x%04x mc_nr 0x%012lx\n",
           mch_hdrrev, mch_sysid, mch_flags, mch_size, mch_rsvd, mch_mc_nr);
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
      printf("ERROR: wrong message count now: 0x%02x before+1: 0x%02x\n",
             cneth_msg_cnt, cneth_msg_cnt_save+1);
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

int proc_pg(uint16_t* word, uint8_t wrd_cnt) {
  for (int i = 0; i <= wrd_cnt-1; i+=3) {      
#ifdef DEBUG
    printf("msg: %04x %04x %04x\n", word[i+2], word[i+1], word[i]);
#endif
  }
  return 0;
}

int process_mc_roc(mc_desc* mc) {
  
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
#endif
  // Check MC header
  if( mch_hdrrev != MCH_HDRREV || mch_sysid != MCH_SYSID || mch_flags != MCH_FLAGS
      || mch_size != MCH_SIZE || mch_rsvd != MCH_RSVD ) {
    printf("ERROR: wrong MC header\n");
    printf("MC header :\n hdrrev 0x%02x, sysid 0x%02x, flags 0x%04x size 0x%08x\n"
           " rsvd 0x%04x mc_nr 0x%012lx\n",
           mch_hdrrev, mch_sysid, mch_flags, mch_size, mch_rsvd, mch_mc_nr);
    error++;
  }
  
  // Check cnet messages
  uint16_t* cnet_word = (uint16_t*) &(mc_word[2]);
  uint8_t cneth_msg_cnt_save = 0;
  unsigned int w = 0;
  while(w < mc_size-8) {
    // first and second 
    uint8_t cneth_wrd_cnt = cnet_word[w] & 0xff;
    uint8_t cneth_msg_cnt = (cnet_word[w] >> 8) & 0xff;
    uint16_t cnet_src_addr = cnet_word[w+1];
#ifdef DEBUG
    printf("msg_cnt: 0x%02x wrd_cnt 0x%02x ROCID 0x%04x\n",
           cneth_msg_cnt, cneth_wrd_cnt, cnet_src_addr);
#endif
   // check message count
    if (cneth_msg_cnt != ((cneth_msg_cnt_save+1) & 0xff) && w > 0) {
      printf("ERROR: wrong message count now: 0x%02x before+1: 0x%02x\n",
             cneth_msg_cnt, cneth_msg_cnt_save+1);
      error++;
    }
    w +=2;
    cneth_msg_cnt_save = cneth_msg_cnt;
    
    // process content
    error += proc_pg(&(cnet_word[w]), cneth_wrd_cnt);

    //set start index for next cnet message 
    // (wrd_cnt is w/o first two words, -4 accounts for padding added)
    w = w+cneth_wrd_cnt-4+(4-(cneth_wrd_cnt+2)%4);
}
  
  return error;
}



//////////////////////////////////////////////////////////////////////////////////

rorcfs_device *dev = NULL;
rorcfs_bar *bar1 = NULL;
cbm_link* cbmLink[CHANNELS] = {NULL};

void fnExit (void)
{
  for(uint_fast8_t i=0; i<CHANNELS; i++) {
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

  for(uint_fast8_t i=0; i<CHANNELS; i++) {
    
    cbmLink[i] = new cbm_link();
    
    if( cbmLink[i]->init(i, dev, bar1) < 0) {
      perror("ERROR: cbmLink->init");
    }
  }
  
  for(uint_fast8_t i=0; i<CHANNELS; i++) {
    if (cbmLink[i]->enable() < 0) {
      perror("ERROR: cbmLink->enable");
    }
  }

  for(uint_fast8_t i=0; i<CHANNELS; i++) {
    eb[i] = (uint64_t *)cbmLink[i]->ebuf()->getMem();
    rb[i] = (struct rb_entry *)cbmLink[i]->rbuf()->getMem();
    chan[i] = cbmLink[i]->get_ch();
  }

  for(uint_fast8_t i=0; i<CHANNELS; i++) {
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
      int error = process_mc_roc(mc);
      error_cnt += error;
      if(error){
	dump_mc(mc);	     
      }
//       if(j % 1000000 == 0) {
//        dump_mc(mc);	     
//        //        dump_mc_raw(eb[i], rb[i], j);
//      //}

      cbmLink[i]->ack_mc();
      if((j & 0xFFFFF) == 0xFFFFF) {
        printf("%d analysed\n", j);
      }

    }
    printf("MCs analysed %d\nTotal errors: %d\n", mc_limit, error_cnt);
  }

  return 0;
}
