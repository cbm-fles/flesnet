#ifndef ZMQ_DEMO_ITEMWORKERPROTOCOL_HPP
#define ZMQ_DEMO_ITEMWORKERPROTOCOL_HPP

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

struct WorkerParameters {
  size_t stride;
  size_t offset;
  WorkerQueuePolicy queue_policy;
  std::string client_name;
};

enum class WorkerMessageType { Register, WorkItem, Heartbeat, Disconnect };

inline std::string register_str(const WorkerParameters& parameters) {
  return "REGISTER " + std::to_string(parameters.stride) + " " +
         std::to_string(parameters.offset) + " " +
         to_string(parameters.queue_policy) + " " + parameters.client_name;
}

inline bool is_work_item(const std::string& message) {
  return (message.rfind("WORK_ITEM ", 0) == 0);
}

inline bool is_heartbeat(const std::string& message) {
  return (message.rfind("HEARTBEAT", 0) == 0);
}

inline bool is_disconnect(const std::string& message) {
  return (message.rfind("DISCONNECT", 0) == 0);
}

#endif
