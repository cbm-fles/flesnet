// Copyright 2012-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <iostream>

#pragma pack(1)

/// InfiniBand request IDs.
enum RequestIdentifierUcx {
  ID_WRITE_DATA = 1,
  ID_WRITE_DATA_WRAP,
  ID_WRITE_DESC,
  ID_SEND_STATUS,
  ID_RECEIVE_STATUS,
  ID_SEND_FINALIZE
};

#pragma pack()

/// Overloaded output operator for RequestIdentifier values.
inline std::ostream& operator<<(std::ostream& s, RequestIdentifierUcx v) {
  switch (v) {
  case ID_WRITE_DATA:
    return s << "ID_WRITE_DATA";
  case ID_WRITE_DATA_WRAP:
    return s << "ID_WRITE_DATA_WRAP";
  case ID_WRITE_DESC:
    return s << "ID_WRITE_DESC";
  case ID_SEND_STATUS:
    return s << "ID_SEND_STATUS";
  case ID_RECEIVE_STATUS:
    return s << "ID_RECEIVE_STATUS";
  case ID_SEND_FINALIZE:
    return s << "ID_SEND_FINALIZE";
  default:
    return s << static_cast<int>(v);
  }
}
