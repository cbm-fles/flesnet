/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#include <iostream>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <pda.h>

#include <dma_buffer.hpp>
#include <device.hpp>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

using namespace std;

namespace pda{

dma_buffer::dma_buffer() { }

dma_buffer::~dma_buffer() {
  m_physical_size = 0;
  m_mapping_size = 0;
  m_mem = NULL;
}

/**
 * Allocate Buffer: initiate memory allocation,
 * connect to new buffer & retrieve actual buffer sizes
 **/
int dma_buffer::allocate(device* device, uint64_t size, uint64_t id,
                         int overmap, int dma_direction) {
//  m_device = device->m_device;
//
//  if (PDA_SUCCESS != PciDevice_allocDMABuffer(m_device, id, size, &m_buffer))
//  { return -1; }
//
//  if (overmap == 1) {
//    if (PDA_SUCCESS != DMABuffer_wrapMap(m_buffer)) {
//      return -1;
//    }
//  }
//
//  return connect(device, id);

    m_alloced_mem = memalign(PAGE_SIZE, size);
    if(m_alloced_mem == NULL)
    { return -1; }

    memset(m_alloced_mem, 0, size);
    return(reg(device, m_alloced_mem, size, id, overmap, dma_direction));

}

int dma_buffer::reg(device* device, void *buf, uint64_t size, uint64_t id,
                         int overmap, int dma_direction) {
  m_device = device->m_device;

  if (PDA_SUCCESS != PciDevice_registerDMABuffer(m_device, id, buf, size, &m_buffer))
  { return -1; }

  if (overmap == 1) {
    if (PDA_SUCCESS != DMABuffer_wrapMap(m_buffer)) {
      return -1;
    }
  }

  return connect(device, id);
}

/**
 * Release buffer
 **/
int dma_buffer::deallocate() {
  if (PciDevice_deleteDMABuffer(m_device, m_buffer) != PDA_SUCCESS) {
    return -1;
  }

  m_id = 0;
  m_mem = NULL;

  if(m_alloced_mem != NULL){
    free(m_alloced_mem);
    m_alloced_mem = NULL;
  }

  return 0;
}

int dma_buffer::connect(device* device, uint64_t id) {
  m_device = device->m_device;
  m_id = id;

  if (m_mem != NULL) {
    errno = EPERM;
    return -1;
  }

  if (DMABuffer_getMap(m_buffer, (void**)(&m_mem)) != PDA_SUCCESS) {
    return -1;
  }

  if (DMABuffer_getLength(m_buffer, &m_physical_size) != PDA_SUCCESS) {
    return -1;
  }

  if (isOvermapped() == 1) {
    m_mapping_size = 2 * m_physical_size;
  } else {
    m_mapping_size = m_physical_size;
  }

  if (DMABuffer_getSGList(m_buffer, &m_sglist) != PDA_SUCCESS) {
    return -1;
  }

  m_scatter_gather_entries = 0;
  for (DMABuffer_SGNode* sg = m_sglist; sg != NULL; sg = sg->next) {
    m_scatter_gather_entries++;
  }

  return 0;
}

int dma_buffer::isOvermapped() {
  void* map_two = NULL;

  if (DMABuffer_getMapTwo(m_buffer, &map_two) != PDA_SUCCESS) {
    if (map_two != NULL) {
      return 1;
    }
  }

  return 0;
}
}
