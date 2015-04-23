/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#include <pci_bar.hpp>
#include <device.hpp>
#include <data_structures.hpp>

#include <pda.h>

using namespace std;

namespace pda {

pci_bar::pci_bar(device* dev, uint8_t number) {
  m_parent_dev = dev;
  m_number = number;
  m_pda_pci_device = m_parent_dev->PDAPciDevice();

  barMap(number);
}

void pci_bar::barMap(uint8_t number) {

  m_pda_bar = NULL;

  if (PciDevice_getBar(m_pda_pci_device, &m_pda_bar, number) != PDA_SUCCESS) {
    throw PdaException("Bar fetching failed!");
  }

  if (Bar_getMap(m_pda_bar, reinterpret_cast<void**>(&m_bar), &m_size) != PDA_SUCCESS) {
    throw PdaException("Bar mapping failed!");
  }
}
}
