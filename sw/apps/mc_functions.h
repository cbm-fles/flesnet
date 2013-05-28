// DUMP functions
void dump_raw(volatile uint64_t *buf, unsigned int size)
{
  unsigned int i = 0;
  for (i = 0; i < size; i+=2) {
    printf("%p: %016lx %016lx\n", (void *)(buf+i), *(buf+i+1), *(buf+i)); 
  }
}

void dump_report(volatile struct rb_entry *rb, unsigned int nr)
{
  printf("Report #%d addr=%p :\n offset=%lu, length=%lu, mc_size=%u Bytes\n",
         nr,
         (void *)&rb[nr],
         rb[nr].offset,
	 rb[nr].length,
	 rb[nr].mc_size);
}

void dump_mc(mc_desc* mc)
{
  printf("Report addr=%p :\n length=%lu, mc_size=%u Bytes\n",
         (void *)mc->rbaddr,
         mc->length,
         mc->size);
  // length and offset is in bytes, adressing is per uint64 
  volatile uint64_t* mc_word = mc->addr;
  printf("Event	 #%ld\n", mc->nr);
  for (unsigned int i = 0; i < ((mc->length)/sizeof(uint64_t)); i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i*8, (void *)&mc_word[i], mc_word[i+1], mc_word[i]);
  }
}

// Dumps mc correponding to entry nr in report buffer
// will not accout for wrapping, no range check
void dump_mc_raw(volatile uint64_t *eb,
                 volatile rb_entry *rb,
                 unsigned int nr)
{
  dump_report(rb, nr);
  printf("Event	 #%d\n", nr);
  // length and offset is in bytes, adressing is per uint64 
  volatile uint64_t* mc_word = eb + rb[nr].offset/sizeof(uint64_t);
  for (unsigned int i = 0; i < (rb[nr].length/sizeof(uint64_t)); i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i*8, (void *)&mc_word[i], mc_word[i+1], mc_word[i]);
  }
}

#define MCH_HDRREV 0x01
#define MCH_SYSID 0xaa
#define MCH_FLAGS 0x0000
#define MCH_SIZE 0x12345678
#define MCH_RSVD 0xabcd

//--------------------------------
int process_mc(mc_desc* mc) {
  
  int error = 0;
  uint64_t mc_nr = mc->nr;
  uint32_t mc_size = mc->size; // bytes
  volatile uint64_t* mc_word = mc->addr;
  // hdr words
  uint64_t hdr0 = mc_word[0];
  uint64_t hdr1 = mc_word[1];
  unsigned int mch_hdrrev = (hdr0 >> 56) & 0xff;
  unsigned int mch_sysid =  (hdr0 >> 48) & 0xff;
  unsigned int mch_flags =  (hdr0 >> 32) & 0xffff;
  unsigned int mch_size =   hdr0 & 0xffffffff;
  uint64_t mch_mc_nr = hdr1;

  if(mch_size == 0) {
    printf("MC has size 0");
    error++;
    return error;
  }

#ifdef DEBUG
  printf("MC nr %ld, addr %p, size %d \n", mc_nr, (void *)mc_word, mc_size);
#endif

  if( mch_hdrrev != MCH_HDRREV || mch_sysid != MCH_SYSID || mch_flags != MCH_FLAGS
      || mch_size != MCH_SIZE ) {
    printf("ERROR: wrong MC header\n");
    printf("MC header :\n hdrrev 0x%02x, sysid 0x%02x, flags 0x%04x size 0x%08x\n"
           " mc_nr 0x%016lx\n",
           mch_hdrrev, mch_sysid, mch_flags, mch_size, mch_mc_nr);
    error++;
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
    uint16_t cnet_src_addr = cnet_word[w+1];
    w +=2;

    // check header message count
    if (cneth_msg_cnt != ((cneth_msg_cnt_save+1) & 0xff) && w > 2) {
      //#ifdef DEBUG
      printf("ERROR: wrong header message count now: 0x%02x before+1: 0x%02x\n",
             cneth_msg_cnt, cneth_msg_cnt_save+1);
      //#endif
      error++;
    }
    cneth_msg_cnt_save = cneth_msg_cnt;
    
    // message ramp
    uint16_t cnet_word_save = 0;
    int i = 0;
    for (i = 0; i <= cneth_wrd_cnt-2; i++) {      
      if (((cnet_word[w+i] & 0xff) != (cnet_word_save & 0xff) + 1 && i > 0) || 
          (cnet_word[w] != 0xbc00 && i == 0) ) {
        //#ifdef DEBUG
        printf("ERROR: wrong cnet word 0x%04x 0x%04x\n", cnet_word[w+i], cnet_word_save);
        //#endif
        error++;
        return error;
      }
      cnet_word_save = cnet_word[w+i];
    }

    // last word
    uint16_t cnet_msg_nr = cnet_word[w+i];
    if (cnet_msg_nr != ((cnet_msg_nr_save+1) & 0xffff) && w > 2) {
      //#ifdef DEBUG
      printf("ERROR: wrong pgen message number now: 0x%04x before+1: 0x%04x\n",
             cnet_msg_nr, cnet_msg_nr_save+1);
      //#endif
      error++;
      return error;
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
  return error;
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


int process_mc_roc(mc_desc* mc) {
  
  int error = 0;
  //  uint64_t mc_nr = mc->nr;
  uint32_t mc_size = mc->size; // bytes
  volatile uint64_t* mc_word = mc->addr;
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
  printf("MC nr %ld, addr %p, size %d \n", mc_nr, (void *)mc_word, mc_size);
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
