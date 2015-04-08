/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#ifndef PCI_DMA_BUFFER_H
#define PCI_DMA_BUFFER_H

#include <cstdint>
#include <data_structures.hpp>

typedef struct DMABuffer_struct DMABuffer;
typedef struct DMABuffer_SGNode_struct DMABuffer_SGNode;

#define DMA_BUFFER_FAULT_MAP       1
#define DMA_BUFFER_FAULT_LENGTH    2
#define DMA_BUFFER_FAULT_SGLIST    3
#define DMA_BUFFER_FAULT_BUFFERREG 4
#define DMA_BUFFER_FAULT_ALLOC     5
#define DMA_BUFFER_FAULT_FREE      6


namespace pda {
class device;

/**
 * @class rorcfs_buffer
 * @brief buffer management class
 *
 * This class manages the DMA receive buffers. One instance of this
 * class represents one couple of EventBuffer and ReportBuffer with
 * their corresponding sysfs attributes
 **/
class dma_buffer {
public:

    /**
     * Allocate buffer: This function initiates allocation of an
     * EventBuffer of [size] bytes with Buffer-ID [id]. The size
     * of the according ReportBuffer is determined by the driver.
     * @param device pointer to parent rorcfs_device instance
     * @param size Size of EventBuffer in bytes
     * @param id Buffer-ID to be used for this buffer. This ID has to
     *        be unique within all instances of rorcfs_buffer on a machine.
     */
     dma_buffer(device* device, uint64_t size, uint64_t id);
     dma_buffer(device* device, void *buf, uint64_t size, uint64_t id);
     dma_buffer(device* device, uint64_t id);

    ~dma_buffer();

    /**
     * Buffer-ID
     * @return unsigned long Buffer-ID
     */
    uint64_t ID() { return m_id; }

    /**
     * Physical Buffer size in bytes. Requested buffer
     * size from init() is rounded up to the next PAGE_SIZE
     * boundary.
     * @return number of bytes allocated as Buffer
     */
    uint64_t physicalSize() { return m_physical_size; }

    /**
     * Size of the EB mapping. THis is double the size of
     * the physical buffer size due to overmapping
     * @return size of the EB mapping in bytes
     */
    uint64_t mappingSize() { return m_mapping_size; }

    /**
     * Is the buffer overmapped or not
     * @return 0 if unset, nonzero if set
     */
    int isOvermapped();

    /**
     * Number of scatter-gather entries for the Buffer
     * @return number of entries
     */
    uint64_t numberOfSGEntries() { return m_scatter_gather_entries; }

    /**
     * return memory buffer
     * @return pointer to mmap'ed buffer memory
     **/
    unsigned int* mem() { return m_mem; }

    /**
     * return SG list
     * @return pointer to scatter gather list
     **/
    DMABuffer_SGNode* sglist() { return m_sglist; }

protected:

    void connect(device* dev, uint64_t id);
    int deallocate();

    PciDevice* m_device        = NULL;
    DMABuffer* m_buffer        = NULL;
    DMABuffer_SGNode* m_sglist = NULL;
    uint64_t m_id = 0;

    unsigned int* m_mem        = NULL;
    uint64_t m_physical_size   = 0;
    uint64_t m_mapping_size    = 0;

    uint64_t m_scatter_gather_entries = 0;
    void *m_alloced_mem = NULL;
};

}
#endif
