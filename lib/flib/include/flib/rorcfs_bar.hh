/**
 * @file
 * @author Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @version 0.1
 * @date 2014-07-03
 *
 * The bar class represents a Base Address Register (BAR) file
 * mapping of the RORCs PCIe address space
 */

#ifndef LIBFLIB_BAR_H
#define LIBFLIB_BAR_H

#include <pda.h>

#define BAR_ERROR_CONSTRUCTOR_FAILED 0

#include "rorcfs_device.hh"
#include <pthread.h>

/**
 * @class
 * @brief Represents a Base Address Register (BAR) file
 * mapping of the RORCs PCIe address space
 */
class rorcfs_bar
{
public:
    /**
     * Constructor that sets fname accordingly. No mapping is
     * performed at this point.
     * @param dev parent rorcfs_device
     * @param n number of BAR to be mapped [0-6]
    **/
     rorcfs_bar(device* dev, uint8_t number);
    ~rorcfs_bar();


    void* get_mem_ptr(){return static_cast<void*>(m_bar);};

    /**
     * Get size of mapped BAR. This value is only valid after init()
     * @return size of mapped BAR in (unsigned long) bytes
     **/
    size_t get_size(){return(m_size);};

protected:
    device          *m_parent_dev;
    PciDevice       *m_pda_pci_device;
    pthread_mutex_t  m_mtx;
    int32_t          m_number;
    uint8_t         *m_bar;
    size_t           m_size;
};

#endif /* LIBFLIB_BAR_H */
