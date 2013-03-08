/**
 * \file Timeslice.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef TIMESLICE_HPP
#define TIMESLICE_HPP


/// Timeslice component descriptor.
typedef struct {
    uint64_t tsNum; ///< Timeslice number.
    uint64_t offset; ///< Start offset (in words) of corresponding data.
    uint64_t size;   ///< Size (in words) of corresponding data.
} TimesliceComponentDescriptor;


/// Structure representing the header of a microslice container.
typedef struct {
    uint8_t hdrrev; ///< Header revision.
    uint8_t sysid;  ///< System ID.
    uint16_t flags; ///< Flags.
    uint32_t size;  ///< Size of the microslice container in words.
    uint64_t time;  ///< Number of previous microslice containers.
} MicrosliceHeader;


/// Structure representing a set of compute node buffer positions.
typedef struct {
    uint64_t data; ///< The position in the data buffer.
    uint64_t desc; ///< The position in the description buffer.
} ComputeNodeBufferPosition;


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
    default:
        return s << (int) v;
    }
}


#endif /* TIMESLICE_HPP */
