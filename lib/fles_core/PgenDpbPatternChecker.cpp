// Copyright 2016, Junfeng Yang <yangjf@ustc.edu.cn>
// Info for pGenDPB pattern checking:
// Timeslices are treated as independet
// Microslices are treated as consecutive data stream
// Checks for:
// - consecutive CBMnet frame numbers across all microclices
//   in one timeslice component
// - cosecutive pgen sequence numbers across all microslices
//   in one timeslice component
// Implementation is not dump parallelizable across ts components!

#include "PgenDpbPatternChecker.hpp"
#include <iostream>
#include <array>

void PgenDpbPatternChecker::reset()
{
    pgen_dpb_flim_id_ = 0xff;
}

bool PgenDpbPatternChecker::check(const fles::Microslice& m)
{
    std::array<uint64_t,MAX_FIFO_NUM_PER_LINK> pgen_dpb_channel_data;    
    uint64_t data_head, flim_id;

    if (m.desc().size %sizeof(uint64_t) !=0) {
        	std::cerr <<"pGen DPB: Package length error! ms_size="<< m.desc().size << " ms start time=" <<
        			m.desc().idx << std::endl;
        	return false;
    }

    // check ramp
    if (m.desc().size >= 8) {
        const uint64_t* content =  reinterpret_cast<const uint64_t*>(m.content()) + 0;
	      data_head = content[0] >> 56;
        if (data_head!=DEFAULT_DATA_HEAD) {
            	std::cerr <<"pGen DPB: data_head error! data_head="<< std::hex << data_head << std::endl;
            	return false;
	      }	  
        flim_id = (content[0] & 0x00ff000000000000) >> 48;
        if (flim_id >=MAX_FLIM_ID) {
            std::cerr <<"pGen DPB: flim_id error! flim_id="<< std::hex << flim_id << std::endl;
            	return false;
        }

	      if (pgen_dpb_flim_id_ ==0xff) {
		        pgen_dpb_flim_id_ = flim_id;
	      //	std::cout << "pGen DPB: flim " << flim_id <<" found!" << std::endl;
	      }
        else if (pgen_dpb_flim_id_ != flim_id) {
	          std::cerr <<"pGen DPB: flim_id error! flim_id="<< std::hex << flim_id << std::endl;
	          return false;
	      }
        
        for(uint64_t i=0; i<MAX_FIFO_NUM_PER_LINK; i++){
		        pgen_dpb_channel_data[i]= (content[0] & 0xffff000000000000) + (i<<40);
          }
        
        size_t pos=0;
        size_t fifo_id=0;
        while (pos < (m.desc().size/ sizeof(uint64_t))) {
            	fifo_id = (content[pos] & 0x0000ff0000000000) >>40;
            if (fifo_id >= MAX_FIFO_NUM_PER_LINK) {
                std::cerr <<"pGen DPB: fifo id error! fifo_id="<< std::hex << fifo_id << std::endl;
	              return false;
            }

            	if (content[pos] != pgen_dpb_channel_data[fifo_id]) {
                std::cerr << "pGen DPB: error in ramp word "
                          << " exp " << std::hex << pgen_dpb_channel_data[fifo_id] << " seen "
                          << content[pos] << std::endl;
                return false;
            }
            	if ((pgen_dpb_channel_data[fifo_id] & 0x000000ffffffffff)== 0xffffffffff) {
            		  pgen_dpb_channel_data[fifo_id] = pgen_dpb_channel_data[fifo_id] & 0xffffff0000000000;
            	} else {
            		  pgen_dpb_channel_data[fifo_id]++;
            	}

            ++pos;
        }
    }

    return true;
}
