/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#ifndef DMA_CHANNEL_H
#define DMA_CHANNEL_H

#include <pda/device.hpp>
#include <pda/dma_buffer.hpp>

#include <data_structures.hpp>

//using namespace pda;

namespace flib {
class register_file_bar;

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
  dma_channel(register_file_bar* rf, pda::device* parent_device,
              size_t dma_transfer_size);

  ~dma_channel();

  /**
  * prepare EventBuffer: copy scatterlist from
  * rorcfs_buffer into the EventBufferDescriptorManager in the RORC
  * @param buf rorcfs_buffer instance to be used as
  *        event destination buffer
  * @return 0 on sucess, -1 on errors, -EFBIG if more
  *         than 2048 sg-entries
  **/
  void prepareEB(pda::dma_buffer* buf);

  /**
  * prepare ReportBuffer: copy scatterlist from
  * rorcfs_buffer into the ReportBufferDescriptorManager
  * in the RORC
  * @param buf rorcfs_buffer instance to be used as
  *        report destination buffer
  * @return 0 on sucess, -1 on errors
  **/
  void prepareRB(pda::dma_buffer* buf);

  /**
  * set Enable Bit of EBDM
  * @param enable nonzero param will enable, zero will disable
  **/
  void enableEB(int enable);

  /**
  * set Enable Bit of RBDM
  * @param enable nonzero param will enable, zero will disable
  **/
  void enableRB(int enable);

  /**
  * Enable Bit of EBDM
  * @return enable bit
  **/
  unsigned int isEBEnabled();

  /**
  * Enable Bit of RBDM
  * @return enable bit
  **/
  unsigned int isRBEnabled();

  void enableDMAEngine(bool enable);

  void rstPKTFifo(bool enable);

  /**
  * @return DMA Packetizer COnfiguration and Status
  **/
  unsigned int DMAConfig();

  /**
  * dma transfer size from current HW configuration
  * @return dma transfer size in bytes
  **/
  size_t dma_transfer_size();

  /**
  * number of Scatter Gather entries for the Event buffer
  * @return number of entries
  **/
  unsigned int EBDMnSGEntries();

  /**
  * number of Scatter Gather entries for the Report buffer
  * @return number of entries
  **/
  unsigned int RBDMnSGEntries();

  /**
  * DMA Packetizer 'Busy' flag
  * @return 1 if busy, 0 if idle
  **/
  unsigned int isDMABusy();

  /**
  * buffer size set in EBDM. This returns the size of the
  * DMA buffer set in the DMA enginge and has to be the physical
  * size of the associated DMA buffer.
  * @return buffer size in bytes
  **/
  unsigned long EBSize();

  /**
  * buffer size set in RBDM. As the RB is not overmapped this size
  * should be equal to the sysfs file size and buf->RBSize()
  * @return buffer size in bytes
  **/
  unsigned long RBSize();

  /**
  * configure DMA engine for current set of buffers
  * @param ebuf pointer to struct rorcfs_buffer to be used as
  * event buffer
  * @param rbuf pointer to struct rorcfs_buffer to be used as
  * report buffer
  * @param dma_transfer_size transfer packet size to be used (in bytes)
  * @return 0 on sucess, <0 on error
  * */
  void configureChannel(pda::dma_buffer* data_buffer,
                        pda::dma_buffer* desc_buffer);

  /**
  * set Event Buffer File Offset
  * the DMA engine only writes to the Event buffer as long as
  * its internal offset is at least (dma_transfer_size)-bytes smaller
  * than the Event Buffer Offset. In order to start a transfer,
  * set EBOffset to (BufferSize-dma_transfer_size).
  * IMPORTANT: offset has always to be a multiple of dma_transfer_size!
  **/
  void setEBOffset(unsigned long offset);

  /**
  * current Event Buffer File Offset
  * @return unsigned long offset
  **/
  unsigned long EBOffset();

  /**
  * set Report Buffer File Offset
  **/
  void setRBOffset(unsigned long offset);

  /**
  * Report Buffer File Offset
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
  * buffer offset that is currently used as
  * DMA destination
  * @return 64bit offset in report buffer file
  **/
  unsigned long RBDMAOffset();

  /**
  * buffer offset that is currently used as
  * DMA destination
  * @return 64bit offset in event buffer file
  **/
  unsigned long EBDMAOffset();

protected:
  register_file_bar* m_rfpkt = NULL;
  pda::device* m_parent_device = NULL;
  size_t m_dma_transfer_size = 0;

  void prepareBuffer(pda::dma_buffer* buf, uint32_t buf_sel);

private:

  void check_dma_transfer_size(size_t dma_transfer_size);

  /**
  * setDMAConfig set the DMA Controller operation mode
  * @param config Bit mapping:
  * TODO
  **/
  void setDMAConfig(unsigned int config);
  
};
}

#endif
