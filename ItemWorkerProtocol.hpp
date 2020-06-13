#ifndef ZMQ_DEMO_ITEMWORKERPROTOCOL_HPP
#define ZMQ_DEMO_ITEMWORKERPROTOCOL_HPP

#include <chrono>
#include <iostream>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <type_traits>

constexpr static auto distributor_heartbeat_interval =
    std::chrono::milliseconds{500};
constexpr static auto distributor_poll_timeout = std::chrono::milliseconds{100};
constexpr static auto worker_poll_timeout = std::chrono::milliseconds{500};
constexpr static auto worker_heartbeat_timeout =
    4 * distributor_heartbeat_interval;

class WorkerProtocolError : public std::runtime_error {
public:
  explicit WorkerProtocolError(const std::string& msg = "")
      : runtime_error(msg) {}
};

using ItemID = size_t;

class Item {
public:
  Item(std::queue<ItemID>* completed_items, ItemID id, std::string payload)
      : completed_items_(completed_items), id_(id),
        payload_(std::move(payload)) {}

  // Item is non-copyable
  Item(const Item& other) = delete;
  Item& operator=(const Item& other) = delete;
  Item(Item&& other) = delete;
  Item& operator=(Item&& other) = delete;

  [[nodiscard]] ItemID id() const { return id_; }

  [[nodiscard]] const std::string& payload() const { return payload_; }

  ~Item() { completed_items_->push(id_); }

private:
  std::queue<ItemID>* completed_items_;
  const ItemID id_;
  const std::string payload_;
};

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

#endif
