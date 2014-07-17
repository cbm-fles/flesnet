#include <iostream>

#include <pda.h>

#include <flib/dma_buffer.hpp>
#include <flib/device.hpp>

using namespace std;

namespace flib
{


    dma_buffer::dma_buffer()
    {

    }

    dma_buffer::~dma_buffer()
    {
        m_physical_size = 0;
        m_mapping_size  = 0;
        m_mem           = NULL;
    }



    /**
     * Allocate Buffer: initiate memory allocation,
     * connect to new buffer & retrieve actual buffer sizes
     **/
    int
    dma_buffer::allocate
    (
        device   *device,
        uint64_t  size,
        uint64_t  id,
        int       overmap,
        int       dma_direction
    )
    {
        m_device = device;

        if
        (
            PDA_SUCCESS !=
                PciDevice_allocDMABuffer
                    (m_device, id, size, PDABUFFER_DIRECTION_BI, &m_buffer)
        )
        { return -1; }

        if(overmap == 1)
        {
            if(PDA_SUCCESS != DMABuffer_wrapMap(m_buffer) )
            { return -1; }
        }

        return connect(device, id);
    }



    /**
     * Release buffer
     **/
    int dma_buffer::deallocate()
    {
        if(DMABuffer_free(m_buffer, PDA_DELETE) != PDA_SUCCESS)
        { return -1; }

        m_id  = 0;
        m_mem = NULL;

        return 0;
    }



    int
    dma_buffer::connect
    (
        device   *device,
        uint64_t  id
    )
    {
        m_device        = device->m_device;
        m_id            = id;

        if(m_mem!=NULL)
        {
            errno = EPERM;
            return -1;
        }

        if( DMABuffer_getMap(m_buffer, (void**)(&m_mem) )!=PDA_SUCCESS )
        { return -1; }

        if( DMABuffer_getLength( m_buffer, &m_physical_size) != PDA_SUCCESS )
        { return -1; }

        if(getOvermapped() == 1)
        { m_mapping_size = 2 * m_physical_size; }
        else
        { m_mapping_size = m_physical_size; }

        if(DMABuffer_getSGList(m_buffer, &m_sglist)!=PDA_SUCCESS)
        { return -1; }

        m_scatter_gather_entries = 0;
        for(DMABuffer_SGNode *sg=m_sglist; sg!=NULL; sg=sg->next)
        { m_scatter_gather_entries++; }

        return 0;
    }

    int
    dma_buffer::getOvermapped()
    {
        void *map_two = NULL;

        if(DMABuffer_getMapTwo(m_buffer, &map_two) != PDA_SUCCESS)
        { if(map_two != NULL){ return 1; } }

        return 0;
    }

}
