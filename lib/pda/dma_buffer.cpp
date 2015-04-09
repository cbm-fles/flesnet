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

using namespace std;

namespace pda{

/** Allocate a new one */
dma_buffer::dma_buffer(device* device, uint64_t size, uint64_t id)
{
    if(PDA_SUCCESS != PciDevice_allocDMABuffer(device->m_device, id, size, &m_buffer)){
      throw PdaException("DMA_BUFFER_FAULT_ALLOC");
    }

    connect(device, id);
}



/** Register a malloced or memaligned buffer */
dma_buffer::dma_buffer(device* device, void* buf, uint64_t size, uint64_t id)
{
    if(PDA_SUCCESS != PciDevice_registerDMABuffer(device->m_device, id, buf, size, &m_buffer)){
      throw PdaException("DMA_BUFFER_FAULT_REG");
    }

    connect(device, id);
}



/** Attach an already existing buffer */
dma_buffer::dma_buffer(device* device, uint64_t id)
{
    if(PDA_SUCCESS != PciDevice_getDMABuffer(device->m_device, id, &m_buffer)){
        throw PdaException("DMA_BUFFER_FAULT_GET");
    }

    connect(device, id);
}



dma_buffer::~dma_buffer()
{
    deallocate();
}



int dma_buffer::isOvermapped()
{
    void* map_two = NULL;

    if(DMABuffer_getMapTwo(m_buffer, &map_two) != PDA_SUCCESS) {
        if(map_two != NULL) {
            return 1;
        }
    }

    return 0;
}


/** protected functions */

void
dma_buffer::connect(device* device, uint64_t id) {
    m_device = device->m_device;
    m_id     = id;

    if(DMABuffer_getMap(m_buffer, (void**)(&m_mem)) != PDA_SUCCESS) {
      throw PdaException("DMA_BUFFER_FAULT_MAP");
    }

    if(DMABuffer_getLength(m_buffer, &m_size) != PDA_SUCCESS) {
        throw PdaException("DMA_BUFFER_FAULT_LENGTH");
    }

    if(DMABuffer_getSGList(m_buffer, &m_sglist) != PDA_SUCCESS) {
      throw PdaException("DMA_BUFFER_FAULT_SGLIST");
    }

    m_scatter_gather_entries = 0;
    for(DMABuffer_SGNode* sg = m_sglist; sg != NULL; sg = sg->next) {
        m_scatter_gather_entries++;
    }
}

void dma_buffer::deallocate(){
    PciDevice_deleteDMABuffer(m_device, m_buffer);
}


}
