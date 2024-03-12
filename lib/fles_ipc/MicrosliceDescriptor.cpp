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

std::ostream& operator<<(std::ostream& out, const fles::MicrosliceDescriptor& desc) {
    out << "hdr_id: " << uint16_t(desc.hdr_id) << std::endl;
    out << "hdr_ver: " << uint16_t(desc.hdr_ver) << std::endl;
    out << "eq_id: " << uint16_t(desc.eq_id) << std::endl;
    out << "flags: " << uint16_t(desc.flags) << std::endl;
    out << "sys_id: " << uint16_t(desc.sys_id) << std::endl;
    out << "sys_ver: " << uint16_t(desc.sys_ver) << std::endl;
    out << "idx: " << uint16_t(desc.idx) << std::endl;
    out << "crc: " << uint16_t(desc.crc) << std::endl;
    out << "offset: " << uint16_t(desc.offset) << std::endl;
    out << "size: " << uint16_t(desc.size);
    return out;
}
}