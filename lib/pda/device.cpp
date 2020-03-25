/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#include "device.hpp"
#include "data_structures.hpp"
#include "device_operator.hpp"

#include <pda.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace std;

namespace pda {
device::device(device_operator* device_operator, int32_t device_index)
    : m_parent_dop(device_operator) {
  if (DeviceOperator_getPciDevice(m_parent_dop->PDADeviceOperator(), &m_device,
                                  device_index) != PDA_SUCCESS) {
    throw PdaException("Device object creation from index failed.");
  }
}

device::device(uint8_t bus, uint8_t device, uint8_t function)
    : m_parent_dop(nullptr) {
  if ((m_device = PciDevice_new(0, bus, device, function)) == NULL) {
    throw PdaException("Device object creation from BDF failed.");
  }
}

device::~device() {
  // do not delete PciDevice if it belongs to DeviceOperator
  // DeviceOperator will do the cleanup on deletion
  if (m_parent_dop == nullptr) {
    if (PciDevice_delete(m_device, PDA_DELETE_PERSISTANT) != PDA_SUCCESS) {
      cout << "Deleting device operator failed!" << endl;
    }
  }
}

uint16_t device::domain() {
  uint16_t domain_id;
  if (PciDevice_getDomainID(m_device, &domain_id) == PDA_SUCCESS) {
    return (domain_id);
  }

  return (0);
}

uint8_t device::bus() {
  uint8_t bus_id;
  if (PciDevice_getBusID(m_device, &bus_id) == PDA_SUCCESS) {
    return (bus_id);
  }

  return (0);
}

uint8_t device::slot() {
  uint8_t device_id;
  if (PciDevice_getDeviceID(m_device, &device_id) == PDA_SUCCESS) {
    return (device_id);
  }

  return (0);
}

uint8_t device::func() {
  uint8_t function_id;
  if (PciDevice_getFunctionID(m_device, &function_id) == PDA_SUCCESS) {
    return (function_id);
  }

  return (0);
}

size_t device::max_payload_size() {
  size_t max_payload_size;
  if (PciDevice_getmaxPayloadSize(m_device, &max_payload_size) == PDA_SUCCESS) {
    return (max_payload_size);
  }
  return (0);
}

} // namespace pda
