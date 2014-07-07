#include <cstring>
#include <string>
#include <iostream>
#include <sstream>

#include <flib/flib_link.hpp>

namespace flib
{
    int
    flib_link::init_hardware()
    {
        // disable packer if still enabled
        enable_cbmnet_packer(false);
        // reset everything to ensure clean startup
        _rst_channel();
        set_start_idx(1);

        /** prepare EventBufferDescriptorManager
         * and ReportBufferDescriptorManage
         * with scatter-gather list
         **/
        if( m_channel->prepareEB(m_event_buffer.get()) < 0 )
        { return -1; }

        if( m_channel->prepareRB(m_dbuffer.get()) < 0 )
        { return -1; }

        if( m_channel->configureChannel(m_event_buffer.get(), m_dbuffer.get(), 128) < 0)
        { return -1; }

        // clear eb for debugging
        memset(m_event_buffer->getMem(), 0, m_event_buffer->getMappingSize());
        // clear rb for polling
        memset(m_dbuffer->getMem(), 0, m_dbuffer->getMappingSize());

        m_eb = (uint64_t *)m_event_buffer->getMem();
        m_db = (struct MicrosliceDescriptor *)m_dbuffer->getMem();

        m_dbentries = m_dbuffer->getMaxRBEntries();

        // Enable desciptor buffers and dma engine
        m_channel->setEnableEB(1);
        m_channel->setEnableRB(1);
        m_channel->setDMAConfig( m_channel->getDMAConfig() | 0x01 );

        return 0;
    }

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
