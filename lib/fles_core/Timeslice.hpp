#pragma once
/**
 * \file Timeslice.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include <iostream>
#include <rdma/rdma_cma.h>

#pragma pack(1)

// Microslice descriptor.
struct MicrosliceDescriptor
{
    uint8_t hdr_id;  // "Header format identifier" (0xDD)
    uint8_t hdr_ver; // "Header format version"    (0x01)
    uint16_t eq_id;  // "Equipment identifier"
    uint16_t flags;  // "Status and error flags"
    uint8_t sys_id;  // "Subsystem identifier"
    uint8_t sys_ver; // "Subsystem format version"
    uint64_t idx;    // "Microslice index"
    uint32_t crc;    // "CRC32 checksum"
    uint32_t size;   // "Content size (bytes)"
    uint64_t offset; // "Offset in event buffer (bytes)"
};

/// Timeslice component descriptor.
struct TimesliceComponentDescriptor
{
    uint64_t ts_num; ///< Timeslice index.
    uint64_t offset; ///< Start offset (in bytes) of corresponding data.
    uint64_t size;   ///< Size (in bytes) of corresponding data.
};

/// Structure representing a set of compute node buffer positions.
struct ComputeNodeBufferPosition
{
    uint64_t data; ///< The position in the data buffer.
    uint64_t desc; ///< The position in the description buffer.
    bool operator<(const ComputeNodeBufferPosition& rhs) const
    {
        return desc < rhs.desc;
    }
    bool operator>(const ComputeNodeBufferPosition& rhs) const
    {
        return desc > rhs.desc;
    }
    bool operator==(const ComputeNodeBufferPosition& rhs) const
    {
        return desc == rhs.desc && data == rhs.data;
    }
    bool operator!=(const ComputeNodeBufferPosition& rhs) const
    {
        return desc != rhs.desc || data != rhs.data;
    }
};

const ComputeNodeBufferPosition CN_WP_FINAL = {UINT64_MAX, UINT64_MAX};

/// InfiniBand request IDs.
enum REQUEST_ID {
    ID_WRITE_DATA = 1,
    ID_WRITE_DATA_WRAP,
    ID_WRITE_DESC,
    ID_SEND_CN_WP,
    ID_RECEIVE_CN_ACK,
    ID_SEND_CN_ACK,
    ID_RECEIVE_CN_WP,
    ID_SEND_FINALIZE
};

/// Access information for a remote memory region.
struct BufferInfo
{
    uint64_t addr; ///< Target memory address
    uint32_t rkey; ///< Target remote access key
};

struct ComputeNodeInfo
{
    BufferInfo data;
    BufferInfo desc;
    uint32_t index;
    uint32_t data_buffer_size_exp;
    uint32_t desc_buffer_size_exp;
};

struct InputNodeInfo
{
    uint32_t index;
};

struct TimesliceWorkItem
{
    uint64_t ts_pos; ///< Start offset (in items) of this timeslice
    uint32_t num_core_microslices;    ///< Number of core microslices
    uint32_t num_overlap_microslices; ///< Number of overlapping microslices
    uint32_t num_components; ///< Number of components (contributing input
    /// channels)
    uint32_t data_buffer_size_exp; ///< Exp. size (in bytes) of each data buffer
    uint32_t desc_buffer_size_exp; ///< Exp. size (in bytes) of each descriptor
                                   /// buffer
};

struct TimesliceCompletion
{
    uint64_t ts_pos;
};

#pragma pack()

/// Overloaded output operator for REQUEST_ID values.
inline std::ostream& operator<<(std::ostream& s, REQUEST_ID v)
{
    switch (v) {
    case ID_WRITE_DATA:
        return s << "ID_WRITE_DATA";
    case ID_WRITE_DATA_WRAP:
        return s << "ID_WRITE_DATA_WRAP";
    case ID_WRITE_DESC:
        return s << "ID_WRITE_DESC";
    case ID_SEND_CN_WP:
        return s << "ID_SEND_CN_WP";
    case ID_RECEIVE_CN_ACK:
        return s << "ID_RECEIVE_CN_ACK";
    case ID_SEND_CN_ACK:
        return s << "ID_SEND_CN_ACK";
    case ID_RECEIVE_CN_WP:
        return s << "ID_RECEIVE_CN_WP";
    case ID_SEND_FINALIZE:
        return s << "ID_SEND_FINALIZE";
    default:
        return s << static_cast<int>(v);
    }
}

inline std::ostream& operator<<(std::ostream& s, ibv_wr_opcode v)
{
    switch (v) {
    case IBV_WR_RDMA_WRITE:
        return s << "IBV_WR_RDMA_WRITE";
    case IBV_WR_RDMA_WRITE_WITH_IMM:
        return s << "IBV_WR_RDMA_WRITE_WITH_IMM";
    case IBV_WR_SEND:
        return s << "IBV_WR_SEND";
    case IBV_WR_SEND_WITH_IMM:
        return s << "IBV_WR_SEND_WITH_IMM";
    case IBV_WR_RDMA_READ:
        return s << "IBV_WR_RDMA_READ";
    case IBV_WR_ATOMIC_CMP_AND_SWP:
        return s << "IBV_WR_ATOMIC_CMP_AND_SWP";
    case IBV_WR_ATOMIC_FETCH_AND_ADD:
        return s << "IBV_WR_ATOMIC_FETCH_AND_ADD";
    default:
        return s << static_cast<int>(v);
    }
}

inline std::ostream& operator<<(std::ostream& s, ibv_send_flags v)
{
    std::string str;
    if (v & IBV_SEND_FENCE)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_FENCE";
    if (v & IBV_SEND_SIGNALED)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_SIGNALED";
    if (v & IBV_SEND_SOLICITED)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_SOLICITED";
    if (v & IBV_SEND_INLINE)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_INLINE";
    if (str.empty())
        return s << static_cast<int>(v);
    else
        return s << str;
}
