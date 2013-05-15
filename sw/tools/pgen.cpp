#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>


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

  std::vector<mc_desc> _mcv;
  
  unsigned int _mc_nr;

  std::mt19937 rng{static_cast<uint64_t> (std::chrono::system_clock::now().time_since_epoch().count())};
  std::uniform_int_distribution<uint32_t> mc_dist{20, 40};
  std::uniform_int_distribution<uint32_t> empty_dist{0, 4};
   
public:

  mc_pgen() : _mc_nr(0) {}

  ~mc_pgen() {}

  mc_desc* get_mc() {

    // create mc_desc
    
    uint32_t mc_size = 0;
    if (empty_dist(rng) != 0) {
      mc_size = mc_dist(rng); // rnd size 64 bit words            
    }
    
    uint64_t* mc = new uint64_t[mc_size];
    
    _mcv.push_back({_mc_nr, mc, mc_size * (uint32_t)sizeof(uint64_t)});

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

    //printf("hdr0 %016lx\n", hdr0);
    //printf("hdr1 %016lx\n", hdr1);

    mc[0] = hdr0;
    mc[1] = hdr1;

    for (uint32_t i = 2; i < mc_size; i++) {
      mc[i] = i; //rnd here
    }

    _mc_nr++;    

    return &_mcv.back();
  }

  int ack_mc() {
    // mc memory
    for (auto& i : _mcv) {
      delete[] i.addr;
    }  
    _mcv.clear();
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
  
  mc_desc* mcp;
  for(int i = 0; i < 200; i++) { 
    mcp = MyPgen->get_mc();
    dump_mc(mcp);
    //    MyPgen->ack_mc();
  }
  MyPgen->ack_mc();

  delete MyPgen;
  return 0;
}


