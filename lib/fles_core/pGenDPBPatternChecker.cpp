// Copyright 2016, Junfeng Yang <yangjf@ustc.edu.cn>
// Info for pGenDPB pattern checking:
// Timeslices are treated as independet
// Microslices are treated as consecutive data stream
// Checks for:
// - consecutive CBMnet frame nubers across all microclices
//   in one timeslice component
// - cosecutive pgen sequence numbers across all microslices
//   in one timeslice component
// Implementation is not dump parallelizable across ts components!

#include "pGenDPBPatternChecker.hpp"
#include <iostream>

void pGenDPBPatternChecker::reset()
{
    for(int i=0; i<MAX_FIFO_NUM_PER_LINK; i++){
    	pgen_dpb_channel_mask_[i]=0;
    }
    pgen_dpb_flim_id_ = 0xff;
}

bool pGenDPBPatternChecker::check(const fles::Microslice& m)
{
    uint64_t pgen_dpb_channel_data[MAX_FIFO_NUM_PER_LINK];
    uint64_t sys_id, flim_id;
/* //do not check lenght to save time

    // ms was truncated
    if (m.desc().size >= 1 &&
        ((m.desc().flags &
          static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) == 0)) {
    	std::cerr << "pGen DPB: MicroSlice was truncated! ms start time="<< m.desc().idx << std::endl;
    	return false;
    }

    if (m.desc().size %sizeof(uint64_t) !=0) {
    	std::cerr <<"pGen DPB: Package length error! ms_size="<< m.desc().size << " ms start time=" <<
    			m.desc().idx << std::endl;
    	return false;
    }
*/
    const uint64_t* content =  reinterpret_cast<const uint64_t*>(m.content()) + 0;
	sys_id = content[0] >> 56;
	flim_id = (content[0] & 0x00ff000000000000) >> 48;

/* // don't check sys_id to save time
	if (sys_id!=DEFAULT_SYS_ID) {
    	std::cerr <<"pGen DPB: sys_id error! sys_id="<< std::hex << sys_id << std::endl;
    	return false;
	}
*/

	if (pgen_dpb_flim_id_ ==0xff) {
		pgen_dpb_flim_id_ = flim_id;
	//	std::cout << "pGen DPB: flim " << flim_id <<" found!" << std::endl;
	}
	else if (pgen_dpb_flim_id_ != flim_id) {
		std::cerr <<"pGen DPB: flim_id error! flim_id="<< std::hex << flim_id << std::endl;
		return false;
	}

	for(uint64_t i=0; i<MAX_FIFO_NUM_PER_LINK; i++){
		pgen_dpb_channel_data[i]= (sys_id <<56) & (flim_id <<48) & (i<<40);
    }

    // check ramp
    if (m.desc().size > 8) {
        size_t pos=0;
        size_t fifo_channel=0;

        while (pos <= (m.desc().size/ sizeof(uint64_t))) {
        	fifo_channel = (content[pos] & 0x0000ff0000000000) >40;
        	if (pgen_dpb_channel_mask_[fifo_channel]==0) {
      //  		std::cout <<"pGen DPB: fifo " << fifo_channel <<" found! ms start time=" << m.desc().idx << std::endl;
        		pgen_dpb_channel_mask_[fifo_channel]=1;
        	}

        	if (content[pos] != pgen_dpb_channel_data[fifo_channel]) {
                std::cerr << "pGen DPB: error in ramp word "
                          << " exp " << std::hex << pgen_dpb_channel_data[fifo_channel] << " seen "
                          << content[pos] << std::endl;
                return false;
            }
        	if ((pgen_dpb_channel_data[fifo_channel] & 0x000000ffffffffff)== 0xffffffffff) {
        		pgen_dpb_channel_data[fifo_channel] = pgen_dpb_channel_data[fifo_channel] & 0xffffff0000000000;
        	} else {
        		pgen_dpb_channel_data[fifo_channel]++;
        	}

            ++pos;
        }
    }

    return true;
}
