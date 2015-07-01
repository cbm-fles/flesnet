
using namespace flib;
// DUMP functions
void dump_raw(volatile uint64_t *buf, unsigned int size)
{
  unsigned int i = 0;
  for (i = 0; i < size; i+=2) {
    printf("%p: %016lx %016lx\n", (void *)(buf+i), *(buf+i+1), *(buf+i)); 
  }
}

void dump_report(volatile struct fles::MicrosliceDescriptor *rb)
{
  printf("Report addr=%p :\n"
         " hdr id  %02x\n"
         " hdr ver %02x\n"
         " eq id   %04x\n"
         " flags   %04x\n"
	 " sys id  %02x\n"
	 " sys ver %02x\n"
	 " idx     %016lx\n"
	 " crc     %08x\n"
	 " size    %08x\n"
         " offset  %016lx\n",
         (void*)rb,
         rb->hdr_id,
	 rb->hdr_ver,
	 rb->eq_id,
	 rb->flags,
	 rb->sys_id,
	 rb->sys_ver,
	 rb->idx,
	 rb->crc,
	 rb->size,
	 rb->offset);
}

void dump_mc(dma_channel::mc_desc_t* mc)
{
  printf("Report addr=%p :\n mc_size=%u Bytes\n",
         (void *)mc->rbaddr,
         mc->size);
  // size and offset is in bytes, adressing is per uint64 
  volatile uint64_t* mc_word = mc->addr;
  printf("Event	 #%ld\n", mc->nr);
  for (unsigned int i = 0; i < ((mc->size)/sizeof(uint64_t))+2; i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i*8, (void *)&mc_word[i], mc_word[i+1], mc_word[i]);
  }
}

void dump_mc_light(dma_channel::mc_desc_t* mc)
{
  printf("Report addr=%p :\n mc_nr %ld mc_size=%u Bytes\n",
         (void *)mc->rbaddr,
         mc->nr,
         mc->size);
}


// Dumps mc correponding to entry nr in report buffer
// will not accout for wrapping, no range check
void dump_mc_raw(volatile uint64_t *eb,
		 volatile fles::MicrosliceDescriptor *rb,
		 unsigned int nr)
{
  dump_report(rb + nr);
  printf("Event	 #%d\n", nr);
  // size and offset is in bytes, adressing is per uint64 
  volatile uint64_t* mc_word = eb + rb[nr].offset/sizeof(uint64_t);
  for (unsigned int i = 0; i <= (rb[nr].size/sizeof(uint64_t)); i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i*8, (void *)&mc_word[i], mc_word[i+1], mc_word[i]);
  }
}

#define MCH_HDR_ID 0xDD
#define MCH_HDR_VER 0x01
#define MCH_FLAGS 0x0000

//--------------------------------
size_t get_size(dma_channel::mc_desc_t* mc) {
  size_t size_bytes = mc->size;
  size_t max_payload_bytes = 128;
  size_t datasize = ((size_bytes-1) | (max_payload_bytes-1))+1;
  return datasize;
}


int process_mc(dma_channel::mc_desc_t* mc) {

  struct __attribute__ ((__packed__)) s_mc_header {
    uint8_t   hdr_id;  // "Header format identifier"
    uint8_t   hdr_ver; // "Header format version"   
    uint16_t  eq_id;   // "Equipment identifier"    
    uint16_t  flags;   // "Status and error flags"   
    uint8_t   sys_id;  // "Subsystem identifier"     
    uint8_t   sys_ver; // "Subsystem format version" 
    uint64_t  idx;     // "Microslice index"
  };
  
  uint32_t mc_size = mc->size; // bytes
  volatile uint64_t* mc_word = mc->addr;
  
  s_mc_header* mch = (s_mc_header*)mc_word;

  if(mc_size == 0) {
    printf("MC has size 0");
    return -1;
  }

#ifdef DEBUG
  printf("MC nr %ld, addr %p, size %d \n", mc_nr, (void *)mc_word, mc_size);
#endif

  if( mch->hdr_id != MCH_HDR_ID || mch->hdr_ver != MCH_HDR_VER
      || mch->flags != MCH_FLAGS ) {
#ifdef DEBUG
    printf("ERROR: wrong MC header\n");
    printf("MC header :\n hdr_id 0x%02x, hdr_ver 0x%02x, eq_id 0x%04x,"
           " flags 0x%04x, sys_id 0x%02x, sys_ver 0x%02x\n"
           " mc_nr 0x%016lx\n",
           mch->hdr_id, mch->hdr_ver, mch->eq_id, mch->flags, mch->sys_id, mch->sys_ver, mc_nr);
#endif
    return -2;
  }
  
  // loop over cnet messages
  uint8_t cneth_msg_cnt_save = 0;
  uint16_t cnet_msg_nr_save = 0;
  volatile uint16_t* cnet_word = (uint16_t*) &(mc_word[2]);
  unsigned int w = 0;

  while(w < (mc_size >> 1)-8) {
    // TODO: save message count for next mc
    // first and second 
    uint8_t cneth_wrd_cnt = cnet_word[w] & 0xff;
    uint8_t cneth_msg_cnt = (cnet_word[w] >> 8) & 0xff;
    //    uint16_t cnet_src_addr = cnet_word[w+1];
    w +=2;

    // check header message count
    if (cneth_msg_cnt != ((cneth_msg_cnt_save+1) & 0xff) && w > 2) {
#ifdef DEBUG
      printf("ERROR: wrong header message count now: 0x%02x before+1: 0x%02x\n",
             cneth_msg_cnt, cneth_msg_cnt_save+1);
#endif
      return -3;
    }
    cneth_msg_cnt_save = cneth_msg_cnt;
    
    // message ramp
    uint16_t cnet_word_save = 0;
    int i = 0;
    for (i = 0; i <= cneth_wrd_cnt-2; i++) {      
      if (((cnet_word[w+i] & 0xff) != (cnet_word_save & 0xff) + 1 && i > 0) || 
          (cnet_word[w] != 0xbc00 && i == 0) ) {
#ifdef DEBUG
        printf("ERROR: wrong cnet word 0x%04x 0x%04x\n", cnet_word[w+i], cnet_word_save);
#endif
        return -4;
      }
      cnet_word_save = cnet_word[w+i];
    }

    // last word
    uint16_t cnet_msg_nr = cnet_word[w+i];
    if (cnet_msg_nr != ((cnet_msg_nr_save+1) & 0xffff) && w > 2) {
#ifdef DEBUG
      printf("ERROR: wrong pgen message number now: 0x%04x before+1: 0x%04x\n",
             cnet_msg_nr, cnet_msg_nr_save+1);
#endif
      return -5;
    }
    cnet_msg_nr_save = cnet_msg_nr;
    
//    if(error) {
//      printf("Cnet word loop finished with errors for:\n");
//      printf("Cnet hdr: possition: %d msg_cnt 0x%02x wrd_cnt 0x%02x src_addr 0x%04x\n",
//             w-2, cneth_msg_cnt, cneth_wrd_cnt, cnet_src_addr);
//      printf("\n");
//    }

    //set start index for next cnet message 
    // (wrd_cnt is w/o first two words, -4 accounts for padding added)
    //w = w+cneth_wrd_cnt-4+(4-(cneth_wrd_cnt+2)%4);
    w = w+i+(4-(w+i)%4); //set start index for next cnet message   
  }
//  if(error) {
//    printf("Process MC finished with errors for:\n");
//    printf("MC nr %ld, addr %p, size %d Bytes\n", mc_nr, (void *)mc_word, mc_size);
//  }
  return 0;
}








////////////////////////////
int proc_roc_pg(volatile uint16_t* word, uint8_t wrd_cnt) {
  for (int i = 0; i <= wrd_cnt-1; i+=3) {      
#ifdef DEBUG
    printf("msg: %04x %04x %04x\n", word[i+2], word[i+1], word[i]);
#endif
  }
  return 0;
}


/*int process_mc_roc(dma_channel::mc_desc_t* mc) {
  
  int error = 0;
  //  uint64_t mc_nr = mc->nr;
  uint32_t mc_size = mc->size; // bytes
  volatile uint64_t* mc_word = mc->addr;
  // hdr words
  uint64_t hdr0 = mc_word[0];
  uint64_t hdr1 = mc_word[1];

#ifdef DEBUG
  printf("MC nr %ld, addr %p, size %d \n", mc_nr, (void *)mc_word, mc_size);
#endif
  
  // Check cnet messages
  volatile uint16_t* cnet_word = (uint16_t*) &(mc_word[2]);
  uint8_t cneth_msg_cnt_save = 0;
  unsigned int w = 0;
  while(w < (mc_size >> 1)-8) {
    // first and second 
    uint8_t cneth_wrd_cnt = cnet_word[w] & 0xff;
    uint8_t cneth_msg_cnt = (cnet_word[w] >> 8) & 0xff;
    //    uint16_t cnet_src_addr = cnet_word[w+1];
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
    error += proc_roc_pg(&(cnet_word[w]), cneth_wrd_cnt);

    //set start index for next cnet message 
    // (wrd_cnt is w/o first two words, -4 accounts for padding added)
    w = w+cneth_wrd_cnt-4+(4-(cneth_wrd_cnt+2)%4);
  }
  
  return error;
}
*/
