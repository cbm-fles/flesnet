#ifndef ZMQ_DEMO_ITEMWORKERPROTOCOL_HPP
#define ZMQ_DEMO_ITEMWORKERPROTOCOL_HPP

#include <chrono>
#include <iostream>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <type_traits>

/**
 * The ItemWorker protocol
 *
 * Each worker ("ItemWorker") connects to the broker ("ItemDistributor"). The
 * worker starts the communication by sending a REGISTER message. After that,
 * the broker can send a WORK_ITEM message at any time, and the worker can send
 * a COMPLETION message at any time. In addition, the broker sends HEARTBEAT
 * messages regularly if there is no outstanding WORK_ITEM. This allows the
 * worker to detect that the broker has died, in which case the worker closes
 * the socket and opens a new connection.
 *
 * The broker keeps track of all connected workers. It detects that a worker has
 * died by the disconnect notification. When this happens, it releases all
 * outstanding WORK_ITEMs for that worker and stops sending messages to it.
 *
 * The REGISTER message contains a specification of the type of items the worker
 * wants to receive (stride, offset). It also specifies the queueing mode.
 */

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

/**
 * The WorkerQueuePolicy specifies the queueing mode. The worker transmits it as
 * part of its initial REGISTER message to the distributor.
 */
enum class WorkerQueuePolicy {
  /**
   * QueueAll: Fully asynchronous, receive all, don't skip.
   *
   * All matching items are immediately sent as a WORK_ITEM to the send queue,
   * or, if this fails, put on an internal waiting_items queue.
   */
  QueueAll,
  /**
   * PrebufferOne:
   *
   * The broker keeps the newest matching item in an internal 1-item queue if
   * the worker is not idle. The item is sent once the broker receives the
   * outstanding completion from the worker.
   */
  PrebufferOne,
  /**
   * Skip:
   *
   * The broker keeps no item queue for this worker. It only sends the item
   * immediately if the worker is idle.
   */
  Skip
};

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
  /**
   * Request every item with sequence number n for which exists m in N:
   * n = m * stride + offset
   */
  size_t stride;
  size_t offset;
  WorkerQueuePolicy queue_policy;
  std::string client_name;
};

#endif
