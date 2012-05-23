/*
 * \file Timeslice.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef TIMESLICE_HPP
#define TIMESLICE_HPP


// timeslice component descriptor
  typedef struct {
    uint64_t ts_num;
    uint64_t offset;
    uint64_t size;
} TimesliceComponentDescriptor;


typedef struct {
    uint8_t hdrrev;
    uint8_t sysid;
    uint16_t flags;
    uint32_t size;
    uint64_t time;
} MicrosliceHeader;


typedef struct {
    uint64_t data;
    uint64_t desc;
} ComputeNodeBufferPosition;


enum REQUEST_ID { ID_WRITE_DATA = 1, ID_WRITE_DATA_WRAP, ID_WRITE_DESC,
                  ID_SEND_CN_WP, ID_RECEIVE_CN_ACK
                };


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
    default:
        return s << (int) v;
    }
}


#endif /* TIMESLICE_HPP */
