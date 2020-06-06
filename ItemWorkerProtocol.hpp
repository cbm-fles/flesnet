#pragma once

#include <iostream>
#include <ostream>
#include <string>
#include <type_traits>

using ItemID = size_t;

enum class WorkerQueuePolicy { QueueAll, PrebufferOne, Skip };

// Stream enum as the underlying integer type
inline std::ostream& operator<<(std::ostream& os, WorkerQueuePolicy val) {
  return os << static_cast<std::underlying_type_t<WorkerQueuePolicy>>(val);
}
inline std::istream& operator>>(std::istream& is, WorkerQueuePolicy& val) {
  std::underlying_type_t<WorkerQueuePolicy> i;
  is >> i;
  val = static_cast<WorkerQueuePolicy>(i);
  return is;
}
inline std::string to_string(WorkerQueuePolicy val) {
  return std::to_string(
      static_cast<std::underlying_type_t<WorkerQueuePolicy>>(val));
}
