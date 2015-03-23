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
using namespace pda;

namespace flib {

dma_channel::dma_channel(register_file_bar* rf, device* parent_device)
    : m_rfpkt(rf), m_MaxPayload(0) {}

dma_channel::~dma_channel() {
  m_rfpkt = NULL;
  m_MaxPayload = 0;
}

int dma_channel::prepareEB(dma_buffer* buf) {
  return (prepareBuffer(buf, RORC_REG_EBDM_N_SG_CONFIG, 0));
}

int dma_channel::prepareRB(dma_buffer* buf) {
  return (prepareBuffer(buf, RORC_REG_RBDM_N_SG_CONFIG, 1));
}

int dma_channel::prepareBuffer(dma_buffer* buf, sys_bus_addr addr,
                               uint32_t flag) {
  assert(m_rfpkt != NULL);
  unsigned int bdcfg = m_rfpkt->reg(addr);

  /**
   *  check that buffers SGList fits into EBDRAM
   */
  if (buf->numberOfSGEntries() > (bdcfg >> 16)) {
    errno = EFBIG;
    return (-EFBIG);
  }

  struct t_sg_entry_cfg sg_entry;
  uint64_t i = 0;
  for (DMABuffer_SGNode* sg = buf->sglist(); sg != NULL; sg = sg->next) {

    sg_entry.sg_addr_low = (uint32_t)(((uint64_t)sg->d_pointer) & 0xffffffff);
    sg_entry.sg_addr_high = (uint32_t)(((uint64_t)sg->d_pointer) >> 32);
    sg_entry.sg_len = (uint32_t)(sg->length & 0xffffffff);
    sg_entry.ctrl = (1 << 31) | (flag << 30) | ((uint32_t)i);

    m_rfpkt->set_mem(RORC_REG_SGENTRY_ADDR_LOW, &sg_entry,
                     sizeof(sg_entry) >> 2);
    i++;
  }

  /** clear following BD entry (required!) **/
  memset(&sg_entry, 0, sizeof(sg_entry));
  m_rfpkt->set_mem(RORC_REG_SGENTRY_ADDR_LOW, &sg_entry, sizeof(sg_entry) >> 2);

  return 0;
}

/**
 * configure DMA engine for the current
 * set of buffers
 * */
int dma_channel::configureChannel(struct dma_buffer* ebuf,
                                  struct dma_buffer* rbuf,
                                  uint32_t max_payload) {
  struct rorcfs_channel_config config;

  // MAX_PAYLOAD has to be provided as #DWs
  // -> divide size by 4
  uint32_t mp_size = max_payload >> 2;

  // N_SG_CONFIG:
  // [15:0] : actual number of sg entries in RAM
  // [31:16]: maximum number of entries
  uint32_t rbdmnsgcfg = m_rfpkt->reg(RORC_REG_RBDM_N_SG_CONFIG);
  uint32_t ebdmnsgcfg = m_rfpkt->reg(RORC_REG_RBDM_N_SG_CONFIG);

  // check if sglist fits into FPGA buffers
  if (((rbdmnsgcfg >> 16) < rbuf->numberOfSGEntries()) |
      ((ebdmnsgcfg >> 16) < ebuf->numberOfSGEntries())) {
    errno = -EFBIG;
    return errno;
  }

  if (max_payload & 0x3) {
    // max_payload must be a multiple of 4 byte
    errno = -EINVAL;
    return errno;
  } else if (max_payload > 1024) {
    errno = -ERANGE;
    return errno;
  }

  config.ebdm_n_sg_config = ebuf->numberOfSGEntries();
  config.ebdm_buffer_size_low = (ebuf->physicalSize()) & 0xffffffff;
  config.ebdm_buffer_size_high = ebuf->physicalSize() >> 32;
  config.rbdm_n_sg_config = rbuf->numberOfSGEntries();
  config.rbdm_buffer_size_low = rbuf->physicalSize() & 0xffffffff;
  config.rbdm_buffer_size_high = rbuf->physicalSize() >> 32;

  config.swptrs.ebdm_software_read_pointer_low =
      (ebuf->physicalSize() - max_payload) & 0xffffffff;
  config.swptrs.ebdm_software_read_pointer_high =
      (ebuf->physicalSize() - max_payload) >> 32;
  config.swptrs.rbdm_software_read_pointer_low =
      (rbuf->physicalSize() - sizeof(struct MicrosliceDescriptor)) &
      0xffffffff;
  config.swptrs.rbdm_software_read_pointer_high =
      (rbuf->physicalSize() - sizeof(struct MicrosliceDescriptor)) >> 32;

  // set new MAX_PAYLOAD size
  config.swptrs.dma_ctrl = (1 << 31) |      // sync software read pointers
                           (mp_size << 16); // set max_payload

  // copy configuration struct to RORC, starting
  // at the address of the lowest register(EBDM_N_SG_CONFIG)
  m_rfpkt->set_mem(RORC_REG_EBDM_N_SG_CONFIG, &config,
                   sizeof(struct rorcfs_channel_config) >> 2);
  m_MaxPayload = max_payload;

  return 0;
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

void dma_channel::setDMAConfig(unsigned int config) {
  m_rfpkt->set_reg(RORC_REG_DMA_CTRL, config);
}

unsigned int dma_channel::DMAConfig() {
  return m_rfpkt->reg(RORC_REG_DMA_CTRL);
}

void dma_channel::setMaxPayload() {
  uint64_t max_payload_size = 0;

  if (PciDevice_getmaxPayloadSize(parent_device->PDAPciDevice(), &max_payload_size) !=
      PDA_SUCCESS) {
    cout << "Maximum payload size reading failed!\n" << endl;
    abort();
  }

  m_MaxPayload = max_payload_size;
}

uint64_t dma_channel::maxPayload() { return m_MaxPayload; }

void dma_channel::setOffsets(unsigned long eboffset, unsigned long rboffset) {
  assert(m_rfpkt != NULL);
  assert(eboffset % m_MaxPayload == 0);
  assert(rboffset % 32 == 0); // 32 is hard coded in HW
  struct rorcfs_buffer_software_pointers offsets;
  offsets.ebdm_software_read_pointer_low = (uint32_t)(eboffset & 0xffffffff);
  offsets.ebdm_software_read_pointer_high =
      (uint32_t)(eboffset >> 32 & 0xffffffff);

  offsets.rbdm_software_read_pointer_low = (uint32_t)(rboffset & 0xffffffff);
  offsets.rbdm_software_read_pointer_high =
      (uint32_t)(rboffset >> 32 & 0xffffffff);

  offsets.dma_ctrl = (1 << 31) |                   // sync pointers
                     ((m_MaxPayload >> 2) << 16) | // max_payload
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
