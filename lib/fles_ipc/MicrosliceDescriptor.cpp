#include "MicrosliceDescriptor.hpp"

namespace fles {

bool MicrosliceDescriptor::operator==(const MicrosliceDescriptor& msd) const {  
    return (hdr_id == msd.hdr_id &&
            hdr_ver == msd.hdr_ver &&
            eq_id == msd.eq_id &&
            flags == msd.flags &&
            sys_id == msd.sys_id &&
            sys_ver == msd.sys_ver &&
            idx == msd.idx &&
            crc == msd.crc &&
            offset == msd.offset &&
            size == msd.size
            );
}

bool MicrosliceDescriptor::operator!=(const MicrosliceDescriptor& msd) const {
    return !(*this == msd);
}

}