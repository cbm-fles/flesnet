/**
 *
 * @file
 * @author Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @date 2014-07-03
 *
 **/

#include<iostream>
#include<cstdlib>
#include<cstring>

#include<pda.h>

#include "../include/flib/rorcfs_device.hh"

using namespace std;


device::device()
{
    const char *pci_ids[] =
    {
        "10dc beef", /* CRORC as registered at CERN */
        NULL         /* Delimiter*/
    };

    if( (m_dop = DeviceOperator_new(pci_ids)) == NULL)
    { throw DEVICE_CONSTRUCTOR_FAILED; }

    if(DeviceOperator_getPciDevice(m_dop, &m_device, 0) != PDA_SUCCESS)
    { throw DEVICE_CONSTRUCTOR_FAILED; }


}

device::~device()
{
    if( DeviceOperator_delete(m_dop, PDA_DELETE_PERSISTANT) != PDA_SUCCESS)
    { cout << "Deleting device operator failed!" << endl; }
}
