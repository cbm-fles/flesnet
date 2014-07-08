#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <memory>

#include <flib/flib_device.hpp>
#include <flib/flib_link.hpp>
#include <flib/register_file_bar.hpp>
#include <flib/device.hpp>
#include <flib/pci_bar.hpp>

namespace flib
{

    flib_device::flib_device(int device_nr)
    {
        /** TODO: add exception handling here */
        m_device = std::unique_ptr<device>(new device(device_nr));
//        if (m_device->init(device_nr) == -1)
//        { throw RorcfsException("Failed to initialize device"); }

        // bind to BAR1
        m_bar = std::unique_ptr<pci_bar>(new pci_bar(m_device.get(), 1));
//        if ( m_bar->init() == -1 )
//        { throw RorcfsException("BAR1 init failed"); }

        // register file access
        m_register_file = std::unique_ptr<register_file_bar>(new register_file_bar(m_bar.get(), 0));

        // enforce correct hw version
        if (!check_hw_ver() | !check_magic_number())
        { throw FlibException("Error in magic number or hardware version! \n Try to rescan PCIe bus and reload kernel module."); }

        // create link objects
        uint8_t num_links = get_num_hw_links();
        for (size_t i=0; i<num_links; i++)
        {
            link.push_back
            (
                std::unique_ptr<flib_link>
                (
                    new flib_link(i, m_device.get(), m_bar.get())
                )
            );
        }
    }



    bool
    flib_device::check_hw_ver()
    {
        uint16_t hw_ver = m_register_file->get_reg(0) >> 16; // RORC_REG_HARDWARE_INFO;
        bool match = false;

        // check if version of hardware is part of suported versions
        for(auto it = hw_ver_table.begin(); it != hw_ver_table.end() && match == false; ++it)
        {
            if(hw_ver == *it)
            { match = true; }
        }

        // check if version of hardware matches exactly version of header
        if (hw_ver != RORC_C_HARDWARE_VERSION)
        { match = false; }
        return match;
    }



    struct build_info
    flib_device::get_build_info()
    {
        build_info info;

        info.date = get_build_date();
        info.rev[0] = m_register_file->get_reg(RORC_REG_BUILD_REV_0);
        info.rev[1] = m_register_file->get_reg(RORC_REG_BUILD_REV_1);
        info.rev[2] = m_register_file->get_reg(RORC_REG_BUILD_REV_2);
        info.rev[3] = m_register_file->get_reg(RORC_REG_BUILD_REV_3);
        info.rev[4] = m_register_file->get_reg(RORC_REG_BUILD_REV_4);
        info.hw_ver = get_hw_ver();
        info.clean = (m_register_file->get_reg(RORC_REG_BUILD_FLAGS) & 0x1);
        return info;
    }



    std::vector<flib_link*>
    flib_device::get_links()
    {
        std::vector<flib_link*> links;
        for(auto& l : link)
        { links.push_back(l.get()); }
        return links;
    }



    void
    flib_device::enable_mc_cnt(bool enable)
    { m_register_file->set_bit(RORC_REG_MC_CNT_CFG, 31, enable); }



    void
    flib_device::set_mc_time(uint32_t time)
    {
        // time: 31 bit wide, in units of 8 ns
        uint32_t reg = m_register_file->get_reg(RORC_REG_MC_CNT_CFG);
        reg = (reg & ~0x7FFFFFFF) | (time & 0x7FFFFFFF);
        m_register_file->set_reg(RORC_REG_MC_CNT_CFG, reg);
    }



    void
    flib_device::send_dlm()
    { m_register_file->set_reg(RORC_REG_DLM_CFG, 1); }



    boost::posix_time::ptime
    flib_device::get_build_date()
    {
        time_t time =
            (static_cast<time_t>(m_register_file->get_reg(RORC_REG_BUILD_DATE_L)) |
            (static_cast<uint64_t>(m_register_file->get_reg(RORC_REG_BUILD_DATE_H))<<32));
        boost::posix_time::ptime t = boost::posix_time::from_time_t(time);
        return t;
    }

    uint16_t
    flib_device::get_hw_ver()
    { return(static_cast<uint16_t>(m_register_file->get_reg(0) >> 16)); /** RORC_REG_HARDWARE_INFO */ }



    std::string
    flib_device::print_build_info()
    {
        build_info build = get_build_info();
        std::stringstream ss;
        ss << "Build Date:     " << build.date << std::endl
           << "Repository Revision: " << std::hex
           << build.rev[4] << build.rev[3] << build.rev[2]
           << build.rev[1] << build.rev[0] << std::endl;

        if (build.clean)
        { ss << "Repository Status:   clean " << std::endl; }
        else
        {
            ss << "Repository Status:   NOT clean " << std::endl
               << "Hardware Version:    " << build.hw_ver;
        }
        return ss.str();
    }



    std::string
    flib_device::get_devinfo()
    {
        std::stringstream ss;
        ss << " Bus  " << static_cast<uint32_t>(m_device->getBus())
           << " Slot " << static_cast<uint32_t>(m_device->getSlot())
           << " Func " << static_cast<uint32_t>(m_device->getFunc());
        return ss.str();
    }

    uint8_t
    flib_device::get_num_hw_links()
    { return(m_register_file->get_reg(RORC_REG_N_CHANNELS) & 0xFF); }

    register_file_bar*
    flib_device::get_rf() const
    { return m_register_file.get(); }

    flib_link&
    flib_device::get_link(size_t n)
    { return *link.at(n); }

    size_t
    flib_device::get_num_links()
    { return link.size(); }

    bool
    flib_device::check_magic_number()
    { return((m_register_file->get_reg(0) & 0xFFFF) == 0x4844); /** RORC_REG_HARDWARE_INFO */}

}
