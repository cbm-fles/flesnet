/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#pragma once

#include <cstdint>
#include <sstream>
#include <vector>

using PciDevice = struct PciDevice_struct;
using DMABuffer = struct DMABuffer_struct;
using DMABuffer_SGNode = struct DMABuffer_SGNode_struct;

namespace pda {

typedef struct {
  void* pointer;
  size_t length;
} sg_entry_t;

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
  dma_buffer(device* device, void* buf, uint64_t size, uint64_t id);
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
  size_t size() { return m_size; }

  /**
   * return memory buffer
   * @return pointer to mmap'ed buffer memory
   **/
  void* mem() { return m_mem; }

  /**
   * return SG list
   * @return verctor of scatter gather list entries
   **/
  std::vector<sg_entry_t> sg_list() { return m_sglist; }

  size_t num_sg_entries() { return m_sglist.size(); };

  std::string print_buffer_info();

private:
  void connect();
  void deallocate();

  DMABuffer* m_buffer = NULL;
  PciDevice* m_device = NULL;
  uint64_t m_id = 0;

  void* m_mem = NULL;
  size_t m_size = 0;
  std::vector<sg_entry_t> m_sglist;
};
} // namespace pda
