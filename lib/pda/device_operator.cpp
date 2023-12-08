/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */
#include "device_operator.hpp"
#include "data_structures.hpp"

#include <pda.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace std;

namespace pda {

std::array<const char*, 3> device_operator::m_pci_ids = {
    "10dc beaf", /* FLES FLIB */
    "10ee f1e5", /* FLES CRI */
    nullptr      /* Delimiter*/
};

device_operator::device_operator() {
  m_dop = DeviceOperator_new(m_pci_ids.data(), PDA_DONT_ENUMERATE_DEVICES);
  if (m_dop == nullptr) {
    throw PdaException(
        "Device operator instantiation failed. Is the kernel module loaded?");
  }
}

device_operator::~device_operator() {
  if (DeviceOperator_delete(m_dop, PDA_DELETE_PERSISTANT) != PDA_SUCCESS) {
    cout << "Deleting device operator failed!" << endl;
  }
}

uint64_t device_operator::device_count() {
  uint64_t count;
  if (DeviceOperator_getPciDeviceCount(m_dop, &count) != PDA_SUCCESS) {
    throw PdaException("Getting device count failed.");
  }
  return count;
}

} // namespace pda
