// Copyright 2014 Dirk Hutter
// Abstract interface class to access flib system bus
#ifndef REGISTER_FILE_HPP
#define REGISTER_FILE_HPP

#include <flib/data_structures.hh>

namespace flib
{
    class register_file
    {
        public:
            virtual ~register_file(){};

            virtual int
            get_mem(sys_bus_addr addr, void *dest, size_t dwords) = 0;

            virtual int
            set_mem(sys_bus_addr addr, const void *source, size_t dwords) = 0;

            virtual uint32_t
            get_reg(sys_bus_addr addr)
            {
                uint32_t val;
                get_mem(addr, static_cast<void*>(&val), 1);
                return val;
            }

            virtual void
            set_reg(sys_bus_addr addr, uint32_t data)
            { set_mem(addr, static_cast<const void*>(&data), 1); }

            virtual bool
            get_bit(sys_bus_addr addr, int pos)
            {
                uint32_t reg = get_reg(addr);
                return (reg & (1<<pos));
            }

            virtual void
            set_bit(sys_bus_addr addr, int pos, bool enable)
            {
                uint32_t reg = get_reg(addr);
                if(enable)
                { set_reg(addr, (reg | (1<<pos))); }
                else
                { set_reg(addr, (reg & ~(1<<pos))); }
            }
    };
}
#endif /** REGISTER_FILE_HPP */
