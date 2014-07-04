#ifndef FLIB_DEVICE_HPP
#define FLIB_DEVICE_HPP

#include"boost/date_time/posix_time/posix_time.hpp"

#include<flib/flib_device.hpp>
#include<flib/flib_link.hpp>
#include<flib/rorcfs_device.hh>

//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace flib
{

    struct build_info
    {
        boost::posix_time::ptime date;
        uint32_t rev[5];
        uint16_t hw_ver;
        bool clean;
    };

    constexpr std::array<uint16_t, 1> hw_ver_table = {{ 3}};

    class flib_device
    {
    public:
         flib_device(int device_nr);
        ~flib_device(){};

        bool check_hw_ver();

        size_t get_num_links()
        { return link.size(); }

        flib_link& get_link(size_t n)
        { return *link.at(n); }

        register_file_bar* get_rf() const
        { return m_register_file.get(); }

        std::vector<flib_link*> get_links();

        void enable_mc_cnt(bool enable)
        { m_register_file->set_bit(RORC_REG_MC_CNT_CFG, 31, enable); }

        void set_mc_time(uint32_t time);

        // global dlm send, rquires link local prepare_dlm beforehand
        void send_dlm()
        { m_register_file->set_reg(RORC_REG_DLM_CFG, 1); }

        boost::posix_time::ptime get_build_date();
        uint16_t                 get_hw_ver();
        struct build_info        get_build_info();
        std::string              print_build_info();
        std::string              get_devinfo();

        uint8_t get_num_hw_links()
        { return(m_register_file->get_reg(RORC_REG_N_CHANNELS) & 0xFF); }

        /**TODO: Dirk, is this really needed to be public? */
        std::vector<std::unique_ptr<flib_link> > link;

    private:

        std::unique_ptr<device>            m_device;
        std::unique_ptr<rorcfs_bar>        m_bar;
        std::unique_ptr<register_file_bar> m_register_file;

        bool check_magic_number()
        { return((m_register_file->get_reg(0) & 0xFFFF) == 0x4844); /** RORC_REG_HARDWARE_INFO */}

    };

} /** namespace flib */

// #pragma GCC diagnostic pop

#endif // FLIB_DEVICE_HPP
