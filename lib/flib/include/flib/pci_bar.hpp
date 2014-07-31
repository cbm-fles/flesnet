/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>, Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @version 0.1
 * @date 2014-07-03
 *
 * The bar class represents a Base Address Register (BAR) file
 * mapping of the RORCs PCIe address space
 */

#ifndef LIBFLIB_BAR_H
#define LIBFLIB_BAR_H

#include <pthread.h>
#include <stdint.h>

typedef struct PciDevice_struct PciDevice;
typedef struct Bar_struct Bar;

namespace flib
{
    class device;

    /**
     * @class
     * @brief Represents a Base Address Register (BAR) file
     * mapping of the FLIBs PCIe address space
     */
    class pci_bar
    {
        public:
            /**
             * @param dev parent rorcfs_device
             * @param n number of BAR to be mapped [0-6]
            **/
             pci_bar(device* dev, uint8_t number);
            ~pci_bar(){};


            void* get_mem_ptr(){return static_cast<void*>(m_bar);};

            /**
             * Get size of mapped BAR.
             * @return size of mapped BAR in bytes
             **/
            size_t get_size(){return(m_size);};

        protected:
            device          *m_parent_dev;
            PciDevice       *m_pda_pci_device;
            Bar             *m_pda_bar;
            pthread_mutex_t  m_mtx;
            uint8_t          m_number;
            uint8_t         *m_bar;
            size_t           m_size;

            void getBarMap(uint8_t number);
    };
} /** namespace flib */

#endif /* LIBFLIB_BAR_H */
