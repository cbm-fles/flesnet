#include <cstring>
#include <string>
#include <iostream>
#include <sstream>

#include <flib/flib_link.hpp>

namespace flib
{
    std::string
    flib_link::get_buffer_info(rorcfs_buffer* buf)
    {
        std::stringstream ss;
        ss  << "start address = "  << buf->getMem() <<", "
            << "physical size = "  << (buf->getPhysicalSize() >> 20) << " MByte, "
            << "mapping size = "   << (buf->getMappingSize()  >> 20) << " MByte, "
            << std::endl
            << "  end address = "  << (void*)((uint8_t *)buf->getMem() + buf->getPhysicalSize()) << ", "
            << "num SG entries = " << buf->getnSGEntries() << ", "
            << "max SG entries = " << buf->getMaxRBEntries();
        return ss.str();
    }


}
