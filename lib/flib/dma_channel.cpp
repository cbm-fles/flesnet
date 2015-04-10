/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */

#include <unistd.h>

#include <cerrno>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <pda.h>

#include <pda/dma_buffer.hpp>
#include <pda/device.hpp>
#include <pda/pci_bar.hpp>

#include <dma_channel.hpp>
#include <registers.h>
#include <register_file_bar.hpp>

using namespace std;
//using namespace pda;

namespace flib {

  dma_channel::dma_channel(register_file_bar* rf,
                           pda::device* parent_device,
                           size_t dma_transfer_size)
    : m_rfpkt(rf), m_parent_device(parent_device),
      m_dma_transfer_size(dma_transfer_size) {
  check_dma_transfer_size(dma_transfer_size);    
  }

//dma_channel::dma_channel(register_file_bar* rf,
//                         pda::device* parent_device,
//                         pda::dma_buffer* data_buffer,
//                         pda::dma_buffer* desc_buffer,
//                         size_t dma_transfer_size
//                         )
//  : m_rfpkt(rf), m_parent_device(parent_device),
//    m_data_buffer(data_buffer), m_desc_buffer(des_buffer)
//    m_dma_transfer_size(dma_transfer_size) {
// 
//  check_dma_transfer_size(dma_transfer_size);
//  
//}
  
dma_channel::~dma_channel() {
}

void dma_channel::prepareEB(pda::dma_buffer* buf) {
  prepareBuffer(buf, 0);
}

void dma_channel::prepareRB(pda::dma_buffer* buf) {
  prepareBuffer(buf, 1);
}

void dma_channel::prepareBuffer(pda::dma_buffer* buf, uint32_t buf_sel) {
  assert(m_rfpkt != NULL);
  assert((buf_sel == 0) | (buf_sel == 1));
  
  // check if sglist fits into FPGA buffers
  // N_SG_CONFIG:
  // [15:0] : actual number of sg entries in RAM
  // [31:16]: maximum number of entries
  uint32_t bdcfg = 0;
  if (buf_sel == 0) {
    bdcfg = m_rfpkt->reg(RORC_REG_EBDM_N_SG_CONFIG);
  } else if (buf_sel == 1) {
    bdcfg = m_rfpkt->reg(RORC_REG_RBDM_N_SG_CONFIG);
  } 
  if (buf->numberOfSGEntries() > (bdcfg >> 16)) {
    throw FlibException("Number of SG entries exceeds availabel HW memory");
  }

  // write sg entries to FPGA
  struct t_sg_entry_cfg sg_entry;
  uint64_t bram_addr = 0;
  for (DMABuffer_SGNode* sg = buf->sglist(); sg != NULL; sg = sg->next) {

    sg_entry.sg_addr_low = (uint32_t)(((uint64_t)sg->d_pointer) & 0xffffffff);
    sg_entry.sg_addr_high = (uint32_t)(((uint64_t)sg->d_pointer) >> 32);
    sg_entry.sg_len = (uint32_t)(sg->length & 0xffffffff);
    sg_entry.ctrl = (1 << 31) | (buf_sel << 30) | ((uint32_t)bram_addr);

    m_rfpkt->set_mem(RORC_REG_SGENTRY_ADDR_LOW, &sg_entry,
                     sizeof(sg_entry) >> 2);
    bram_addr++;
  }
  /** clear following BD entry (required!) **/
  memset(&sg_entry, 0, sizeof(sg_entry));
  // TODO fixme ctrl not set!!!
  m_rfpkt->set_mem(RORC_REG_SGENTRY_ADDR_LOW, &sg_entry,
                   sizeof(sg_entry) >> 2);
}

  // TODO private
void dma_channel::check_dma_transfer_size(size_t dma_transfer_size) {
  // dma_transfer_size must be a multiple of 4 byte
  assert ((dma_transfer_size & 0x3) == 0);
  // dma_transfer_size must smaler absolute limit
  assert (dma_transfer_size <= 1024);
  if (dma_transfer_size > m_parent_device->max_payload_size()) {
    throw FlibException("DMA transfer size exceeds PCI MaxPayload");
  }
}
  
/**
 * configure DMA engine for the current
 * set of buffers
 * */
void dma_channel::configureChannel(pda::dma_buffer* data_buffer,
                                   pda::dma_buffer* desc_buffer) {

  struct rorcfs_channel_config config;
  config.ebdm_n_sg_config = data_buffer->numberOfSGEntries();
  config.ebdm_buffer_size_low = (data_buffer->size()) & 0xffffffff;
  config.ebdm_buffer_size_high = data_buffer->size() >> 32;
  config.rbdm_n_sg_config = desc_buffer->numberOfSGEntries();
  config.rbdm_buffer_size_low = desc_buffer->size() & 0xffffffff;
  config.rbdm_buffer_size_high = desc_buffer->size() >> 32;

  config.swptrs.ebdm_software_read_pointer_low =
      (data_buffer->size() - m_dma_transfer_size) & 0xffffffff;
  config.swptrs.ebdm_software_read_pointer_high =
      (data_buffer->size() - m_dma_transfer_size) >> 32;
  config.swptrs.rbdm_software_read_pointer_low =
      (desc_buffer->size() - sizeof(struct MicrosliceDescriptor)) &
      0xffffffff;
  config.swptrs.rbdm_software_read_pointer_high =
      (desc_buffer->size() - sizeof(struct MicrosliceDescriptor)) >> 32;

  // set new DMA_TRANSFER_SIZE size (has to be provided as #DWs)
  uint32_t dma_size_dw = m_dma_transfer_size >> 2;
  config.swptrs.dma_ctrl = (1 << 31) |          // sync software read pointers
                           (dma_size_dw << 16); // set dma_transfer_size

  // copy configuration struct to RORC, starting
  // at the address of the lowest register(EBDM_N_SG_CONFIG)
  m_rfpkt->set_mem(RORC_REG_EBDM_N_SG_CONFIG, &config,
                   sizeof(struct rorcfs_channel_config) >> 2);

}
  
void dma_channel::enableEB(int enable) {
  unsigned int bdcfg = m_rfpkt->reg(RORC_REG_DMA_CTRL);
  if (enable)
    m_rfpkt->set_reg(RORC_REG_DMA_CTRL, (bdcfg | (1 << 2)));
  else
    m_rfpkt->set_reg(RORC_REG_DMA_CTRL, (bdcfg & ~(1 << 2)));
}

unsigned int dma_channel::isEBEnabled() {
  return (m_rfpkt->reg(RORC_REG_DMA_CTRL) >> 2) & 0x01;
}

void dma_channel::enableRB(int enable) {
  unsigned int bdcfg = m_rfpkt->reg(RORC_REG_DMA_CTRL);
  if (enable)
    m_rfpkt->set_reg(RORC_REG_DMA_CTRL, (bdcfg | (1 << 3)));
  else
    m_rfpkt->set_reg(RORC_REG_DMA_CTRL, (bdcfg & ~(1 << 3)));
}

unsigned int dma_channel::isRBEnabled() {
  return (m_rfpkt->reg(RORC_REG_DMA_CTRL) >> 3) & 0x01;
}

void dma_channel::enableDMAEngine(bool enable) {
  m_rfpkt->set_bit(RORC_REG_DMA_CTRL, 0, enable);
}
  
void dma_channel::rstPKTFifo(bool enable) {
  m_rfpkt->set_bit(RORC_REG_DMA_CTRL, 1, enable);
}

void dma_channel::setDMAConfig(unsigned int config) {
  m_rfpkt->set_reg(RORC_REG_DMA_CTRL, config);
}

unsigned int dma_channel::DMAConfig() {
  return m_rfpkt->reg(RORC_REG_DMA_CTRL);
}

size_t dma_channel::dma_transfer_size() { return m_dma_transfer_size; }

void dma_channel::setOffsets(unsigned long eboffset, unsigned long rboffset) {
  assert(m_rfpkt != NULL);
  assert(eboffset % m_dma_transfer_size == 0);
  assert(rboffset % 32 == 0); // 32 is hard coded in HW
  struct rorcfs_buffer_software_pointers offsets;
  offsets.ebdm_software_read_pointer_low = (uint32_t)(eboffset & 0xffffffff);
  offsets.ebdm_software_read_pointer_high =
      (uint32_t)(eboffset >> 32 & 0xffffffff);

  offsets.rbdm_software_read_pointer_low = (uint32_t)(rboffset & 0xffffffff);
  offsets.rbdm_software_read_pointer_high =
      (uint32_t)(rboffset >> 32 & 0xffffffff);

  offsets.dma_ctrl = (1 << 31) |                   // sync pointers
                     ((m_dma_transfer_size >> 2) << 16) | // dma_transfer_size
                     (1 << 2) |                    // enable EB
                     (1 << 3) |                    // enable RB
                     (1 << 0);                     // enable DMA engine

  m_rfpkt->set_mem(RORC_REG_EBDM_SW_READ_POINTER_L, &offsets,
                   sizeof(offsets) >> 2);
}

void dma_channel::setEBOffset(unsigned long offset) {
  assert(m_rfpkt != NULL);
  unsigned int status;

  m_rfpkt->set_mem(RORC_REG_EBDM_SW_READ_POINTER_L, &offset,
                   sizeof(offset) >> 2);
  status = m_rfpkt->reg(RORC_REG_DMA_CTRL);
  m_rfpkt->set_reg(RORC_REG_DMA_CTRL, status | (1 << 31));
}

unsigned long dma_channel::EBOffset() {
  unsigned long offset =
      ((unsigned long)m_rfpkt->reg(RORC_REG_EBDM_SW_READ_POINTER_H) << 32);
  offset += (unsigned long)m_rfpkt->reg(RORC_REG_EBDM_SW_READ_POINTER_L);
  return offset;
}

unsigned long dma_channel::EBDMAOffset() {
  unsigned long offset =
      ((unsigned long)m_rfpkt->reg(RORC_REG_EBDM_FPGA_WRITE_POINTER_H)
       << 32);
  offset += (unsigned long)m_rfpkt->reg(RORC_REG_EBDM_FPGA_WRITE_POINTER_L);
  return offset;
}

void dma_channel::setRBOffset(unsigned long offset) {
  assert(m_rfpkt != NULL);
  unsigned int status;

  m_rfpkt->set_mem(RORC_REG_RBDM_SW_READ_POINTER_L, &offset,
                   sizeof(offset) >> 2);
  status = m_rfpkt->reg(RORC_REG_DMA_CTRL);

  status = m_rfpkt->reg(RORC_REG_DMA_CTRL);
  m_rfpkt->set_reg(RORC_REG_DMA_CTRL, status | (1 << 31));
}

unsigned long dma_channel::RBOffset() {
  unsigned long offset =
      ((unsigned long)m_rfpkt->reg(RORC_REG_RBDM_SW_READ_POINTER_H) << 32);
  offset += (unsigned long)m_rfpkt->reg(RORC_REG_RBDM_SW_READ_POINTER_L);
  return offset;
}

unsigned long dma_channel::RBDMAOffset() {
  unsigned long offset =
      ((unsigned long)m_rfpkt->reg(RORC_REG_RBDM_FPGA_WRITE_POINTER_H)
       << 32);
  offset += (unsigned long)m_rfpkt->reg(RORC_REG_RBDM_FPGA_WRITE_POINTER_L);
  return offset;
}

unsigned int dma_channel::EBDMnSGEntries() {
  return (m_rfpkt->reg(RORC_REG_EBDM_N_SG_CONFIG) & 0x0000ffff);
}

unsigned int dma_channel::RBDMnSGEntries() {
  return (m_rfpkt->reg(RORC_REG_RBDM_N_SG_CONFIG) & 0x0000ffff);
}

unsigned int dma_channel::isDMABusy() {
  return ((m_rfpkt->reg(RORC_REG_DMA_CTRL) >> 7) & 0x01);
}

unsigned long dma_channel::EBSize() {
  unsigned long size =
      ((unsigned long)m_rfpkt->reg(RORC_REG_EBDM_BUFFER_SIZE_H) << 32);
  size += (unsigned long)m_rfpkt->reg(RORC_REG_EBDM_BUFFER_SIZE_L);
  return size;
}

unsigned long dma_channel::RBSize() {
  unsigned long size =
      ((unsigned long)m_rfpkt->reg(RORC_REG_RBDM_BUFFER_SIZE_H) << 32);
  size += (unsigned long)m_rfpkt->reg(RORC_REG_RBDM_BUFFER_SIZE_L);
  return size;
}
}
