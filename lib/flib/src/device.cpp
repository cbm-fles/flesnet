#include <iostream>
#include <cstdlib>
#include <cstring>

#include <pda.h>

#include <flib/device.hpp>

using namespace std;

namespace flib
{
    device::device(int32_t device_index)
    {
        const char *pci_ids[] =
        {
            "10dc beef", /* CRORC as registered at CERN */
            NULL         /* Delimiter*/
        };

        if( (m_dop = DeviceOperator_new(pci_ids)) == NULL)
        { throw DEVICE_CONSTRUCTOR_FAILED; }

        if(DeviceOperator_getPciDevice(m_dop, &m_device, device_index) != PDA_SUCCESS)
        { throw DEVICE_CONSTRUCTOR_FAILED; }
    }

    device::~device()
    {
        if( DeviceOperator_delete(m_dop, PDA_DELETE_PERSISTANT) != PDA_SUCCESS)
        { cout << "Deleting device operator failed!" << endl; }
    }



    uint16_t
    device::getDomain()
    {
        uint16_t domain_id;
        if(PciDevice_getDomainID(m_device, &domain_id) == PDA_SUCCESS)
        { return(domain_id); }

        return(0);
    }

    uint8_t
    device::getBus()
    {
        uint8_t bus_id;
        if(PciDevice_getBusID(m_device, &bus_id) == PDA_SUCCESS)
        { return(bus_id); }

        return(0);
    }

    uint8_t
    device::getSlot()
    {
        uint8_t device_id;
        if(PciDevice_getDeviceID(m_device, &device_id) == PDA_SUCCESS)
        { return(device_id); }

        return(0);
    }

    uint8_t
    device::getFunc()
    {
        uint8_t function_id;
        if(PciDevice_getFunctionID(m_device, &function_id) == PDA_SUCCESS)
        { return(function_id); }

        return(0);
    }
}
