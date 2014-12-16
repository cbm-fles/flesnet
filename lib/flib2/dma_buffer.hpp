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

namespace flib {
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
  friend class dma_channel;

  dma_buffer();
  ~dma_buffer();

  /**
   * Allocate buffer: This function initiates allocation of an
   * EventBuffer of [size] bytes with Buffer-ID [id]. The size
   * of the according ReportBuffer is determined by the driver.
   * @param dev pointer to parent rorcfs_device instance
   * @param size Size of EventBuffer in bytes
   * @param id Buffer-ID to be used for this buffer. This ID has to
   *        be unique within all instances of rorcfs_buffer on a machine.
   * @param overmap enables overmapping of the physical pages if nonzero
   * @param dma_direction select from RORCFS_DMA_FROM_DEVICE,
   *        RORCFS_DMA_TO_DEVICE, RORCFS_DMA_BIDIRECTIONAL
   * @return 0 on sucess, -1 on error
   */
  int allocate(device* dev, uint64_t size, uint64_t id, int overmap,
               int dma_direction);

  /**
   * Free Buffer: This functions initiates de-allocation of the
   * attaced DMA buffers
   * @return 0 on sucess, <0 on error ( use perror() )
   */
  int deallocate();

  /**
   * Connect to an existing buffer
   * @param dev parent rorcfs device
   * @param id buffer ID of exisiting buffer
   * @return 0 on sucessful connect, -EPERM or -ENOMEM on errors
   */
  int connect(device* dev, uint64_t id);

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
   * Maximum number of report buffer entries in the RB
   * @return maximum number of report buffer entries
   */
  uint64_t maxRBEntries() {
    return (m_physical_size / sizeof(struct rorcfs_event_descriptor));
  }

  /**
   * return memory buffer
   * @return pointer to mmap'ed buffer memory
   **/
  unsigned int* mem() { return m_mem; }

protected:
  PciDevice* m_device = NULL;
  DMABuffer* m_buffer = NULL;
  DMABuffer_SGNode* m_sglist = NULL;
  uint64_t m_id = 0;

  unsigned int* m_mem = NULL;
  uint64_t m_physical_size = 0;
  uint64_t m_mapping_size = 0;

  uint64_t m_scatter_gather_entries = 0;
};
}
#endif
