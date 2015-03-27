/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#include <cstdint>
#include <unistd.h>

#include <string>
#include <stdexcept>

typedef uint64_t sys_bus_addr;

/** default maximum payload size in bytes. Check the capabilities
 * of the chipset and the FPGA PCIe core before modifying this value
 * Common values are 128 or 256 bytes.*/
#define MAX_PAYLOAD 128

namespace flib2 {
class FlibException : public std::runtime_error {
public:
  explicit FlibException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

struct mc_desc {
  uint64_t nr;
  volatile uint64_t* addr;
  uint32_t size; // bytes
  volatile uint64_t* rbaddr;
};

struct __attribute__((__packed__)) hdr_config {
  uint16_t eq_id;  // "Equipment identifier"
  uint8_t sys_id;  // "Subsystem identifier"
  uint8_t sys_ver; // "Subsystem format version"
};

struct ctrl_msg {
  uint32_t words; // num 16 bit data words
  uint16_t data[32];
};
}

// has to be 256 Bit, this is hard coded in hw
struct __attribute__((__packed__)) MicrosliceDescriptor {
  uint8_t hdr_id;  // "Header format identifier" DD
  uint8_t hdr_ver; // "Header format version"    01
  uint16_t eq_id;  // "Equipment identifier"
  uint16_t flags;  // "Status and error flags"
  uint8_t sys_id;  // "Subsystem identifier"
  uint8_t sys_ver; // "Subsystem format version"
  uint64_t idx;    // "Microslice index"
  uint32_t crc;    // "CRC32 checksum"
  uint32_t size;   // "Size bytes"
  uint64_t offset; // "Ofsset in event buffer"
};

/** struct holding both read pointers and the
 * DMA engine configuration register contents **/
struct rorcfs_buffer_software_pointers {
  /** EBDM read pointer low **/
  uint32_t ebdm_software_read_pointer_low;
  /** EBDM read pointer high **/
  uint32_t ebdm_software_read_pointer_high;
  /** RBDM read pointer low **/
  uint32_t rbdm_software_read_pointer_low;
  /** RBDM read pointer high **/
  uint32_t rbdm_software_read_pointer_high;
  /** DMA control register **/
  uint32_t dma_ctrl;
};

/** struct rorcfs_channel_config **/
struct rorcfs_channel_config {
  /** EBDM number of sg entries **/
  uint32_t ebdm_n_sg_config;
  /** EBDM buffer size low (in bytes) **/
  uint32_t ebdm_buffer_size_low;
  /** EBDM buffer size high (in bytes) **/
  uint32_t ebdm_buffer_size_high;
  /** RBDM number of sg entries **/
  uint32_t rbdm_n_sg_config;
  /** RBDM buffer size low (in bytes) **/
  uint32_t rbdm_buffer_size_low;
  /** RBDM buffer size high (in bytes) **/
  uint32_t rbdm_buffer_size_high;
  /** struct for read pointers nad control register **/
  struct rorcfs_buffer_software_pointers swptrs;
};

/** struct t_sg_entry_cfg **/
struct t_sg_entry_cfg {
  /** lower part of sg address **/
  uint32_t sg_addr_low;
  /** higher part of sg address **/
  uint32_t sg_addr_high;
  /** total length of sg entry in bytes **/
  uint32_t sg_len;
  /** BDM control register: [31]:we, [30]:sel, [29:0]BRAM addr **/
  uint32_t ctrl;
};

#pragma GCC diagnostic pop
