/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <pda.h>

#include <device_operator.hpp>
#include <data_structures.hpp>

using namespace std;

namespace pda {

const char* device_operator::m_pci_ids[] = {
    "10dc beaf", /* CRORC as registered at CERN */
    NULL         /* Delimiter*/
};

device_operator::device_operator() {
  if ((m_dop = DeviceOperator_new(m_pci_ids, PDA_ENUMERATE_DEVICES)) == NULL) {
    throw PdaException("Device operator instantiation failed.");
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

} // namespace
