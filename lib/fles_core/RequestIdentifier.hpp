// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <iostream>

#pragma pack(1)

/// InfiniBand request IDs.
enum RequestIdentifier {
    ID_WRITE_DATA = 1,
    ID_WRITE_DATA_WRAP,
    ID_WRITE_DESC,
    ID_SEND_CN_WP,
    ID_RECEIVE_CN_ACK,
    ID_SEND_CN_ACK,
    ID_RECEIVE_CN_WP,
    ID_SEND_FINALIZE
};

#pragma pack()

/// Overloaded output operator for RequestIdentifier values.
inline std::ostream& operator<<(std::ostream& s, RequestIdentifier v)
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
