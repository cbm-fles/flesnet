/**
 * @file
 * @author Dominic Eschweiler <dominic.eschweiler@cern.ch>
 * @date 2014-07-08
 **/

#ifndef PCI_DMA_BUFFER_H
#define PCI_DMA_BUFFER_H

#include <stdint.h>
#include <flib/data_structures.hpp>

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
  int allocate(device *dev, uint64_t size, uint64_t id, int overmap,
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
  int connect(device *dev, uint64_t id);

  /**
   * get Buffer-ID
   * @return unsigned long Buffer-ID
   */
  uint64_t getID() { return m_id; }

  /**
   * Get physical Buffer size in bytes. Requested buffer
   * size from init() is rounded up to the next PAGE_SIZE
   * boundary.
   * @return number of bytes allocated as Buffer
   */
  uint64_t getPhysicalSize() { return m_physical_size; }

  /**
   * Get size of the EB mapping. THis is double the size of
   * the physical buffer size due to overmapping
   * @return size of the EB mapping in bytes
   */
  uint64_t getMappingSize() { return m_mapping_size; }

  /**
   * get the overmapped flag of the buffer
   * @return 0 if unset, nonzero if set
   */
  int getOvermapped();

  /**
   * Get number of scatter-gather entries for the Buffer
   * @return number of entries
   */
  uint64_t getnSGEntries() { return m_scatter_gather_entries; }

  /**
   * Get the maximum number of report buffer entries in the RB
   * @return maximum number of report buffer entries
   */
  uint64_t getMaxRBEntries() {
    return (m_physical_size / sizeof(struct rorcfs_event_descriptor));
  }

  /**
   * get memory buffer
   * @return pointer to mmap'ed buffer memory
   **/
  unsigned int *getMem() { return m_mem; }

protected:
  PciDevice *m_device = NULL;
  DMABuffer *m_buffer = NULL;
  DMABuffer_SGNode *m_sglist = NULL;
  uint64_t m_id = 0;

  unsigned int *m_mem = NULL;
  uint64_t m_physical_size = 0;
  uint64_t m_mapping_size = 0;

  uint64_t m_scatter_gather_entries = 0;
};
}
#endif
