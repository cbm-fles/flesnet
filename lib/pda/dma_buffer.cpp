/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#include "dma_buffer.hpp"
#include "data_structures.hpp"
#include "device.hpp"

#include <pda.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <malloc.h>
#include <unistd.h>

using namespace std;

namespace pda {

/** Allocate a new one */
dma_buffer::dma_buffer(device* device, uint64_t size, uint64_t id) {
  if (PDA_SUCCESS !=
      PciDevice_allocDMABuffer(device->m_device, id, size, &m_buffer)) {
    throw PdaException("DMA_BUFFER_FAULT_ALLOC");
  }

  m_device = device->m_device;
  m_id = id;
  connect();
}

/** Register a malloced or memaligned buffer */
dma_buffer::dma_buffer(device* device, void* buf, uint64_t size, uint64_t id) {
  if (PDA_SUCCESS !=
      PciDevice_registerDMABuffer(device->m_device, id, buf, size, &m_buffer)) {
    throw PdaException("DMA_BUFFER_FAULT_REG");
  }

  m_device = device->m_device;
  m_id = id;
  connect();
}

/** Attach an already existing buffer */
dma_buffer::dma_buffer(device* device, uint64_t id) {
  if (PDA_SUCCESS != PciDevice_getDMABuffer(device->m_device, id, &m_buffer)) {
    throw PdaException("DMA_BUFFER_FAULT_GET");
  }

  m_device = device->m_device;
  m_id = id;
  connect();
}

dma_buffer::~dma_buffer() { deallocate(); }

std::string dma_buffer::print_buffer_info() {
  std::stringstream ss;
  ss << "start address = " << m_mem << ", "
     << "physical size = " << (m_size >> 20) << " MByte, " << std::endl
     << "  end address = "
     << static_cast<void*>(static_cast<uint8_t*>(m_mem) + m_size - 1) << ", "
     << "num SG entries = " << m_sglist.size() << std::endl
     << "SG entries (max. 5):" << std::endl;
  for (size_t i = 0; i < 5 && i < m_sglist.size(); ++i) {
    ss << "entry " << i << " start: " << m_sglist.at(i).pointer << " end: "
       << static_cast<void*>(static_cast<uint8_t*>(m_sglist.at(i).pointer) +
                             m_sglist.at(i).length - 1)
       << " length: " << m_sglist.at(i).length;
  }
  return ss.str();
}

/** protected functions */

void dma_buffer::connect() {

  // get pointer to memory
  if (DMABuffer_getMap(m_buffer, reinterpret_cast<void**>(&m_mem)) !=
      PDA_SUCCESS) {
    throw PdaException("DMA_BUFFER_FAULT_MAP");
  }

  // get buffer size
  if (DMABuffer_getLength(m_buffer, &m_size) != PDA_SUCCESS) {
    throw PdaException("DMA_BUFFER_FAULT_LENGTH");
  }

  // get and prepare sg list
  DMABuffer_SGNode* sglist = NULL;
  if (DMABuffer_getSGList(m_buffer, &sglist) != PDA_SUCCESS) {
    throw PdaException("DMA_BUFFER_FAULT_SGLIST");
  }
  sg_entry_t entry;
  for (DMABuffer_SGNode* sg = sglist; sg != NULL; sg = sg->next) {
    entry.pointer = sg->d_pointer;
    entry.length = sg->length;
    m_sglist.push_back(entry);
  }
}

void dma_buffer::deallocate() { PciDevice_deleteDMABuffer(m_device, m_buffer); }
} // namespace pda
