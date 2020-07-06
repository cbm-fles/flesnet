// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include <iostream>

#pragma pack(1)

namespace tl_libfabric {
/// InfiniBand request IDs.
enum RequestIdentifier {
  ID_WRITE_DATA = 1,
  ID_WRITE_DATA_WRAP,
  ID_WRITE_DESC,
  ID_SEND_STATUS,
  ID_RECEIVE_STATUS,
  ID_SEND_FINALIZE,
  ID_HEARTBEAT_SEND_STATUS,
  ID_HEARTBEAT_RECEIVE_STATUS
};
} // namespace tl_libfabric
#pragma pack()

namespace tl_libfabric {
/// Overloaded output operator for RequestIdentifier values.
inline std::ostream& operator<<(std::ostream& s, RequestIdentifier v) {
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
  case ID_HEARTBEAT_SEND_STATUS:
    return s << "ID_HEARTBEAT_SEND_STATUS";
  case ID_HEARTBEAT_RECEIVE_STATUS:
    return s << "ID_HEARTBEAT_RECEIVE_STATUS";
  default:
    return s << static_cast<int>(v);
  }
}
} // namespace tl_libfabric
