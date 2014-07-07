#include <cstring>
#include <string>
#include <iostream>
#include <sstream>

#include <flib/flib_link.hpp>

namespace flib
{

    int
    flib_link::send_dcm(const struct ctrl_msg* msg)
    {
        // TODO: could also implement blocking call
        //       and check if sending is done at the end

        assert(msg->words >= 4 && msg->words <= 32);

        // check if send FSM is ready (bit 31 in r_ctrl_tx = 0)
        if ( (m_rfgtx->get_reg(RORC_REG_GTX_CTRL_TX) & (1<<31)) != 0 )
        { return -1; }

        // copy msg to board memory
        size_t bytes = msg->words*2 + (msg->words*2)%4;
        m_rfgtx->set_mem(RORC_MEM_BASE_CTRL_TX, (const void*)msg->data, bytes>>2);

        // start send FSM
        uint32_t ctrl_tx = 0;
        ctrl_tx          = 1<<31 | (msg->words-1);
        m_rfgtx->set_reg(RORC_REG_GTX_CTRL_TX, ctrl_tx);

        return 0;
    }

    int
    flib_link::recv_dcm(struct ctrl_msg* msg)
    {

        int ret = 0;
        uint32_t ctrl_rx = m_rfgtx->get_reg(RORC_REG_GTX_CTRL_RX);
        msg->words = (ctrl_rx & 0x1F)+1;

        // check if a msg is available
        if((ctrl_rx & (1<<31)) == 0)
        { return -1; }
        // check if received words are in boundary
        // append or truncate if not
        if(msg->words < 4 || msg->words > 32)
        {
            msg->words = 32;
            ret = -2;
        }

        // read msg from board memory
        size_t bytes = msg->words*2 + (msg->words*2)%4;
        m_rfgtx->get_mem(RORC_MEM_BASE_CTRL_RX, (void*)msg->data, bytes>>2);

        // acknowledge msg
        m_rfgtx->set_reg(RORC_REG_GTX_CTRL_RX, 0);

        return ret;
    }

    void
    flib_link::prepare_dlm(uint8_t type, bool enable)
    {
        uint32_t reg = 0;
        if (enable)
        { reg = (1<<4) | (type & 0xF); }
        else
        { reg = (type & 0xF); }
        m_rfgtx->set_reg(RORC_REG_GTX_DLM, reg);
    }

    void
    flib_link::send_dlm()
    {
        // TODO implemet local register in HW
        // this causes all prepared links to send
        m_rfglobal->set_reg(RORC_REG_DLM_CFG, 1);
    }

    uint8_t
    flib_link::recv_dlm()
    {
        // get dlm rx reg
        uint32_t reg = m_rfgtx->get_reg(RORC_REG_GTX_DLM);
        uint8_t type = (reg>>5) & 0xF;
        // clear dlm rx reg
        m_rfgtx->set_bit(RORC_REG_GTX_DLM, 31, true);
        return type;
    }



/*** SETTER ***/
    void
    flib_link::set_data_rx_sel(data_rx_sel rx_sel)
    {
        uint32_t dp_cfg = m_rfgtx->get_reg(RORC_REG_GTX_DATAPATH_CFG);
        switch(rx_sel)
        {
            case disable : m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg & ~3)); break;
            case link    : m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | (1<<1)) & ~1) ); break;
            case pgen    : m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, (dp_cfg | 3) ); break;
            case emu     : m_rfgtx->set_reg(RORC_REG_GTX_DATAPATH_CFG, ((dp_cfg | 1)) & ~(1<<1)); break;
        }
    }

    void
    flib_link::set_hdr_config(const struct hdr_config* config)
    { m_rfgtx->set_mem(RORC_REG_GTX_MC_GEN_CFG_HDR, (const void*)config, sizeof(hdr_config)>>2); }



/*** GETTER ***/
    data_rx_sel
    flib_link::get_data_rx_sel()
    {
        uint32_t dp_cfg = m_rfgtx->get_reg(RORC_REG_GTX_DATAPATH_CFG);
        return static_cast<data_rx_sel>(dp_cfg & 0x3);
    }

    std::string
    flib_link::get_ebuf_info()
    { return get_buffer_info(m_event_buffer.get()); }

    std::string
    flib_link::get_dbuf_info()
    { return get_buffer_info(m_dbuffer.get()); }

    rorcfs_buffer*
    flib_link::ebuf() const
    { return m_event_buffer.get(); }

    rorcfs_buffer*
    flib_link::rbuf() const
    { return m_dbuffer.get(); }

    rorcfs_dma_channel*
    flib_link::get_ch() const
    { return m_channel.get(); }

    register_file_bar*
    flib_link::get_rfpkt() const
    { return m_rfpkt.get(); }

    register_file_bar*
    flib_link::get_rfgtx() const
    { return m_rfgtx.get(); }



/*** PROTECTED ***/

    std::unique_ptr<rorcfs_buffer>
    flib_link::create_buffer(size_t idx, size_t log_size)
    {
        unsigned long size = (((unsigned long)1) << log_size);
        std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());
        if (buffer->allocate(m_device, size, 2*m_link_index+idx, 1, RORCFS_DMA_FROM_DEVICE)!=0)
        {
            if (errno == EEXIST)
            { throw FlibException("Buffer already exists, not allowed to open in create only mode"); }
            else
            { throw RorcfsException("Buffer allocation failed"); }
        }
        return buffer;
    }


    std::unique_ptr<rorcfs_buffer>
    flib_link::open_buffer(size_t idx)
    {
        std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());
        if ( buffer->connect(m_device, 2*m_link_index+idx) != 0 )
        { throw RorcfsException("Connect to buffer failed"); }
        return buffer;
    }


    std::unique_ptr<rorcfs_buffer>
    flib_link::open_or_create_buffer(size_t idx, size_t log_size)
    {
        unsigned long size = (((unsigned long)1) << log_size);
        std::unique_ptr<rorcfs_buffer> buffer(new rorcfs_buffer());

        if (buffer->allocate(m_device, size, 2*m_link_index+idx, 1, RORCFS_DMA_FROM_DEVICE)!=0)
        {
            if (errno == EEXIST)
            {
                if ( buffer->connect(m_device, 2*m_link_index+idx) != 0 )
                { throw RorcfsException("Buffer open failed"); }
            }
            else
            { throw RorcfsException("Buffer allocation failed"); }
        }

        return buffer;
    }


    void
    flib_link::reset_channel()
    {
        // datapath reset, will also cause hw defaults for
        // - pending mc  = 0
        m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, true);
        // rst packetizer fifos
        m_channel->setDMAConfig(0X2);
        // release datapath reset
        m_rfgtx->set_bit(RORC_REG_GTX_DATAPATH_CFG, 2, false);
    }


    void
    flib_link::stop()
    {
        if(m_channel && m_dma_initialized )
        {
            // disable packer
            enable_cbmnet_packer(false);
            // disable DMA Engine
            m_channel->setEnableEB(0);
            // wait for pending transfers to complete (dma_busy->0)
            while( m_channel->getDMABusy() )
            { usleep(100); }

            // disable RBDM
            m_channel->setEnableRB(0);
            // reset
            reset_channel();
        }
    }


    int
    flib_link::init_hardware()
    {
        // disable packer if still enabled
        enable_cbmnet_packer(false);
        // reset everything to ensure clean startup
        reset_channel();
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
