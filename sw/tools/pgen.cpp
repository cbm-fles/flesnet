#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#define MCH_HDRREV 0x01
#define MCH_SYSID 0xaa
#define MCH_FLAGS 0xffff
#define MCH_SIZE 0x12345678
#define MCH_RSVD 0xabcd


struct mc_desc {
  uint64_t nr;
  uint64_t* addr;
  uint32_t size; // bytes
};

class mc_pgen {

  struct mc_desc* _mcb;
  
  unsigned int _index;
  unsigned int _last_index;
  unsigned int _mc_nr;
  unsigned int _wrap;
  
  unsigned int _mcb_entries;
  
public:

  mc_pgen() : _index(0), _last_index(0), _mc_nr(0), _wrap(0) {}

  ~mc_pgen() {
    delete _mcb;
  }

  int init(const unsigned int mcb_entries) {
    _mcb_entries = mcb_entries;
    // create report buffer
    _mcb = new mc_desc[_mcb_entries];
    return 0;
  }


  mc_desc* get_mc() {
    // check index vs pointer

    // create mc_desc
    const uint32_t mc_size = 20; // 64 bit words
    uint64_t* mc = new uint64_t[mc_size];
    
    _mcb[_index].nr = _mc_nr;
    _mcb[_index].addr = mc;
    _mcb[_index].size = mc_size*sizeof(uint64_t);

    // create mc content
    uint8_t mch_hdrrev = MCH_HDRREV;
    uint8_t mch_sysid = MCH_SYSID;
    uint16_t mch_flags = MCH_FLAGS;
    uint32_t mch_size = MCH_SIZE;
    uint16_t mch_rsvd = MCH_RSVD;
    uint64_t mch_mc_nr = _mc_nr; // is 48 bit

    uint64_t hdr0 = 0;
    uint64_t hdr1 = 0;

    hdr0 = hdr0 | ((uint64_t)mch_hdrrev << 56);
    hdr0 = hdr0 | ((uint64_t)mch_sysid << 48);
    hdr0 = hdr0 | ((uint64_t)mch_flags << 32);
    hdr0 = hdr0 | mch_size;
    hdr1 = hdr1 | ((uint64_t)mch_rsvd << 48);
    hdr1 = hdr1 | (mch_mc_nr & 0xffffffffffff);

    printf("hdr0 %016lx\n", hdr0);
    printf("hdr1 %016lx\n", hdr1);

    mc[0] = hdr0;
    mc[1] = hdr1;
 
    mc[2] = _index;
    mc[3] = _wrap;

    for (uint32_t i = 4; i < mc_size; i++) {
      mc[i] = i; //rnd here
    }

    // calculate next desciptor index
    _last_index = _index;
    if( _index < _mcb_entries-1 ) 
      _index++;
    else {
      _wrap++;
      _index = 0;
    }
    _mc_nr++;    

    return &_mcb[_last_index];
  }

  int ack_mc() {
    // set SW read pointer

    // delete mc
    delete[] _mcb[_last_index].addr;
    _mcb[_last_index].size = 0;


    return 0;
  }



};



void dump_mc(mc_desc* mc)
{
  printf("mc_nr=%ld, mc_size=%u Bytes\n",
         mc->nr, mc->size);
  uint64_t* mc_word = mc->addr;
  printf("Event	 #%ld\n", mc->nr);
  for (unsigned int i = 0; i < ((mc->size)/8); i+=2) {
    printf("%4d addr=%p:    %016lx %016lx\n",
	   i*8, (void *)&mc_word[i], mc_word[i+1], mc_word[i]);
  }
}

int main ()
{

  mc_pgen* MyPgen = new mc_pgen();
  
  MyPgen->init(5);

  mc_desc* mcp;
  for(int i = 0; i < 24; i++) { 
    mcp = MyPgen->get_mc();
    dump_mc(mcp);
    MyPgen->ack_mc();
  }

  return 0;
}


