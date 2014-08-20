/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#ifndef DMA_CHANNEL_H
#define DMA_CHANNEL_H

#include <flib/data_structures.hpp>

namespace flib {
class register_file_bar;
class dma_buffer;
class device;

/**
 * @class
 * @brief DMA channel management class
 *
 * Initialize DMA channel with init() before using any other member
 * function. Initialization sets the parent BAR and the channel offset
 * within the BAR (via dma_base). Use prepareEB() and prepareRB() to
 * fill the buffer descriptor memories, then configure Buffer-Enable
 * and -Continuous flags (setRBDMEnable(), setEBDMEnable(),
 * setRBDMContinuous(), setEBDMContinuous() ).
 * No DMA transfer will start unless setDMAEnable() has been called
 * with enable=1.
 **/

class dma_channel {
public:
  dma_channel(register_file_bar* rf, device* parent_device);

  ~dma_channel();

  /**
  * prepare EventBuffer: copy scatterlist from
  * rorcfs_buffer into the EventBufferDescriptorManager in the RORC
  * @param buf rorcfs_buffer instance to be used as
  *        event destination buffer
  * @return 0 on sucess, -1 on errors, -EFBIG if more
  *         than 2048 sg-entries
  **/
  int prepareEB(dma_buffer* buf);

  /**
  * prepare ReportBuffer: copy scatterlist from
  * rorcfs_buffer into the ReportBufferDescriptorManager
  * in the RORC
  * @param buf rorcfs_buffer instance to be used as
  *        report destination buffer
  * @return 0 on sucess, -1 on errors
  **/
  int prepareRB(dma_buffer* buf);

  /**
  * set Enable Bit of EBDM
  * @param enable nonzero param will enable, zero will disable
  **/
  void setEnableEB(int enable);

  /**
  * set Enable Bit of RBDM
  * @param enable nonzero param will enable, zero will disable
  **/
  void setEnableRB(int enable);

  /**
  * Enable Bit of EBDM
  * @return enable bit
  **/
  unsigned int EBEnabled();

  /**
  * Enable Bit of RBDM
  * @return enable bit
  **/
  unsigned int RBEnabled();

  /**
  * setDMAConfig set the DMA Controller operation mode
  * @param config Bit mapping:
  * TODO
  **/
  void setDMAConfig(unsigned int config);

  /**
  * getDMAConfig
  * @return DMA Packetizer COnfiguration and Status
  **/
  unsigned int getDMAConfig();

  /**
  * get maximum payload size from current HW configuration
  * @return maximum payload size in bytes
  **/
  uint64_t getMaxPayload();

  /**
  * get number of Scatter Gather entries for the Event buffer
  * @return number of entries
  **/
  unsigned int getEBDMnSGEntries();

  /**
  * get number of Scatter Gather entries for the Report buffer
  * @return number of entries
  **/
  unsigned int getRBDMnSGEntries();

  /**
  * get DMA Packetizer 'Busy' flag
  * @return 1 if busy, 0 if idle
  **/
  unsigned int getDMABusy();

  /**
  * get buffer size set in EBDM. This returns the size of the
  * DMA buffer set in the DMA enginge and has to be the physical
  * size of the associated DMA buffer.
  * @return buffer size in bytes
  **/
  unsigned long getEBSize();

  /**
  * get buffer size set in RBDM. As the RB is not overmapped this size
  * should be equal to the sysfs file size and buf->getRBSize()
  * @return buffer size in bytes
  **/
  unsigned long getRBSize();

  /**
  * configure DMA engine for current set of buffers
  * @param ebuf pointer to struct rorcfs_buffer to be used as
  * event buffer
  * @param rbuf pointer to struct rorcfs_buffer to be used as
  * report buffer
  * @param max_payload maximum payload size to be used (in bytes)
  * @return 0 on sucess, <0 on error
  * */
  int configureChannel(struct dma_buffer* ebuf, struct dma_buffer* rbuf,
                       uint32_t max_payload);

  /**
  * set Event Buffer File Offset
  * the DMA engine only writes to the Event buffer as long as
  * its internal offset is at least (MaxPayload)-bytes smaller
  * than the Event Buffer Offset. In order to start a transfer,
  * set EBOffset to (BufferSize-MaxPayload).
  * IMPORTANT: offset has always to be a multiple of MaxPayload!
  **/
  void setEBOffset(unsigned long offset);

  /**
  * get current Event Buffer File Offset
  * @return unsigned long offset
  **/
  unsigned long getEBOffset();

  /**
  * set Report Buffer File Offset
  **/
  void setRBOffset(unsigned long offset);

  /**
  * get Report Buffer File Offset
  * @return unsigned long offset
  **/
  unsigned long RBOffset();

  /**
  * setOffsets
  * @param eboffset byte offset in event buffer
  * @param rboffset byte offset in report buffer
  **/
  void setOffsets(unsigned long eboffset, unsigned long rboffset);

  /**
  * get buffer offset that is currently used as
  * DMA destination
  * @return 64bit offset in report buffer file
  **/
  unsigned long RBDMAOffset();

  /**
  * get buffer offset that is currently used as
  * DMA destination
  * @return 64bit offset in event buffer file
  **/
  unsigned long EBDMAOffset();

protected:
  register_file_bar* m_rfpkt = NULL;
  device* parent_device = NULL;
  uint64_t m_MaxPayload = 0;

  /**
   * setMaxPayload( int size ) and setMaxPayload()
   * are wrappers around _setMaxPayload and should
   * be called instead
   */
  void setMaxPayload();

  int prepareBuffer(dma_buffer* buf, sys_bus_addr addr, uint32_t flag);
};
}

#endif
