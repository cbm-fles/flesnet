/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>, Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @version 0.1
 * @date 2014-07-03
 */

#ifndef PCI_DEVICE_H
#define PCI_DEVICE_H

typedef struct DeviceOperator_struct DeviceOperator;
typedef struct PciDevice_struct PciDevice;

namespace flib
{

    #define DEVICE_CONSTRUCTOR_FAILED 0

    /**
     * @class
     * @brief Represents a FLIB PCIe device
     **/
    class device
    {
        friend class dma_buffer;
        friend class dma_channel;

    public:
        device(int32_t device_index);
        ~device();

        uint16_t getDomain();

        /**
         * Get PCIe Bus-ID
         * @return uint8 Bus-ID
        **/
        uint8_t getBus();

        /**
         * Get PCIe Slot-ID
         * @return uint8 Slot-ID
        **/
        uint8_t getSlot();

        /**
         * Get PCIe Function-ID
         * @return uint8 Function-ID
        **/
        uint8_t getFunc();

        /**
         * get PCI-Device
         * @return PCI-Device-Pointer
        **/
        PciDevice *getPdaPciDevice()
        { return(m_device); }

    protected:
        DeviceOperator *m_dop;
        PciDevice      *m_device;
    };
}

#endif /** DEVICE_H */
