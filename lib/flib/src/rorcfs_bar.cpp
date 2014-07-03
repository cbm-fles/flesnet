#include<iostream>
#include<cstdlib>
#include<cstring>

#include "../include/flib/rorcfs.h"
#include "../include/flib/rorcfs_bar.hh"
#include "../include/flib/rorcfs_device.hh"
#include "../include/flib/rorc_registers.h"

using namespace std;

rorcfs_bar::rorcfs_bar
(
    device *dev,
    uint8_t number
)
{
    m_parent_dev     = dev;
    m_number         = number;
    m_pda_pci_device = m_parent_dev->getPdaPciDevice();
//    m_bar            = m_parent_dev->getBarMap(m_number);
//    m_size           = m_parent_dev->getBarSize(m_number);

    if(m_bar == NULL)
    { throw BAR_ERROR_CONSTRUCTOR_FAILED; }

    pthread_mutex_init(&m_mtx, NULL);
}
