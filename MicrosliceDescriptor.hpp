// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>
#include <boost/serialization/access.hpp>

namespace fles
{

#pragma pack(1)

//! Microslice descriptor struct.
struct MicrosliceDescriptor
{
    uint8_t hdr_id;  ///< Header format identifier (0xDD)
    uint8_t hdr_ver; ///< Header format version (0x01)
    uint16_t eq_id;  ///< Equipment identifier
    uint16_t flags;  ///< Status and error flags
    uint8_t sys_id;  ///< Subsystem identifier
    uint8_t sys_ver; ///< Subsystem format version
    uint64_t idx;    ///< Microslice index
    uint32_t crc;    ///< CRC32 checksum
    uint32_t size;   ///< Content size (bytes)
    uint64_t offset; ///< Offset in event buffer (bytes)

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int /* version */)
    {
        ar& hdr_id;
        ar& hdr_ver;
        ar& eq_id;
        ar& flags;
        ar& sys_id;
        ar& sys_ver;
        ar& idx;
        ar& crc;
        ar& size;
        ar& offset;
    }
};

#pragma pack()

} // namespace fles {
