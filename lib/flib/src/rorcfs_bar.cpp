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

    getBarMap(number);

    pthread_mutex_init(&m_mtx, NULL);
}

void
rorcfs_bar::getBarMap(uint8_t number)
{

    m_pda_bar = NULL;

    if(PciDevice_getBar(m_pda_pci_device, &m_pda_bar, number) != PDA_SUCCESS)
    { throw BAR_ERROR_CONSTRUCTOR_FAILED; }

    if(Bar_getMap(m_pda_bar, &m_bar, &m_size) != PDA_SUCCESS)
    { throw BAR_ERROR_CONSTRUCTOR_FAILED; }
}
