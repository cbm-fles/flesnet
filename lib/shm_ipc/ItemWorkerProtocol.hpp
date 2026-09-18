#ifndef SHM_IPC_ITEMWORKERPROTOCOL_HPP
#define SHM_IPC_ITEMWORKERPROTOCOL_HPP

#include "ItemID.hpp"
#include "log.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

/**
 * The item worker protocol, version 2
 *
 * A worker ("ItemWorker") connects to the broker ("ItemDistributor") and opens
 * the conversation with a REGISTER message stating the protocol version and
 * the kind of items it wants to receive. The broker answers with WELCOME,
 * which is the only acknowledgement that the registration succeeded. After
 * that, the broker sends a WORK_ITEM whenever it has one for this worker, and
 * the worker answers every work item with a COMPLETE message. The broker sends
 * END_OF_STREAM after the last item a worker will receive.
 *
 * Both peers send a HEARTBEAT whenever they have not sent anything else for
 * heartbeat_interval, and consider the other side dead after peer_timeout.
 * This is the only liveness mechanism; in particular, neither side relies on a
 * transport-level disconnect notification. A worker that loses the broker
 * closes its socket and reconnects after a randomized backoff, and a broker
 * that loses a worker releases all items outstanding for it. Either peer may
 * send DISCONNECT to end the conversation in an orderly way.
 *
 * Messages consist of a verb frame followed by one frame per argument:
 *
 *   worker -> broker
 *     REGISTER <version> <stride> <offset> <queue_policy> <group_id>
 *              <window> <client_name>
 *     COMPLETE <id> [<id> ...]
 *     HEARTBEAT
 *     DISCONNECT <reason>
 *
 *   broker -> worker
 *     WELCOME <version> <instance_id> <heartbeat_interval_ms> <liveness>
 *     WORK_ITEM <id> [<payload>]
 *     END_OF_STREAM
 *     HEARTBEAT
 *     DISCONNECT <reason>
 *
 * The producer talks to the broker over a separate socket, using the same
 * framing:
 *
 *   producer -> broker      WORK_ITEM <id> [<payload>] | END_OF_STREAM
 *   broker -> producer      COMPLETE <id>
 *
 * The version frame comes first in REGISTER so that a mismatch is detected
 * before anything else is parsed. Peers of different protocol versions do not
 * interoperate; the broker rejects them with a DISCONNECT naming both
 * versions.
 */

/// Version of the item worker protocol implemented here.
constexpr uint32_t item_protocol_version = 2;

// Verbs sent by a worker to the broker
constexpr char register_verb[] = "REGISTER";
constexpr char complete_verb[] = "COMPLETE";
// Verbs sent by the broker to a worker
constexpr char welcome_verb[] = "WELCOME";
constexpr char work_item_verb[] = "WORK_ITEM";
constexpr char end_of_stream_verb[] = "END_OF_STREAM";
// Verbs sent in both directions
constexpr char heartbeat_verb[] = "HEARTBEAT";
constexpr char disconnect_verb[] = "DISCONNECT";

/// Interval at which a peer sends a heartbeat if it has sent nothing else.
constexpr auto heartbeat_interval = std::chrono::milliseconds{1000};
/// Number of missed heartbeat intervals after which a peer is considered dead.
constexpr int heartbeat_liveness = 3;
/// Time without any message after which a peer is considered dead.
constexpr auto peer_timeout = heartbeat_liveness * heartbeat_interval;
/// Time without any message after which a peer is reported as degraded. One
/// full interval is expected between heartbeats, so this has to be longer.
constexpr auto degraded_timeout = 2 * heartbeat_interval;
/// Poll timeout, short enough to keep the heartbeat interval accurate.
constexpr auto poll_timeout = heartbeat_interval / 4;
/// Bounds of the randomized backoff between reconnection attempts.
constexpr auto reconnect_interval_min = std::chrono::milliseconds{100};
constexpr auto reconnect_interval_max = std::chrono::milliseconds{2000};

/// Default upper bound on the items queued for a single worker.
constexpr size_t default_max_queued_items = 1024;

class WorkerProtocolError : public std::runtime_error {
public:
  explicit WorkerProtocolError(const std::string& msg = "")
      : runtime_error(msg) {}
};

/// Parse a decimal number frame.
inline size_t parse_number_frame(const std::string& frame, const char* what) {
  try {
    size_t pos = 0;
    const unsigned long long value = std::stoull(frame, &pos);
    if (pos != frame.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return static_cast<size_t>(value);
  } catch (const std::exception&) {
    throw WorkerProtocolError(std::string("invalid ") + what +
                              " frame: " + frame);
  }
}

/**
 * Destination for the completion of an item.
 *
 * An Item reports itself as completed when it is destroyed, which may happen
 * on a different thread than the one that owns the sink. Implementations are
 * held through a shared_ptr, so that an Item outliving the object that issued
 * it stays safe.
 */
class CompletionSink {
public:
  CompletionSink() = default;
  CompletionSink(const CompletionSink&) = delete;
  CompletionSink& operator=(const CompletionSink&) = delete;
  CompletionSink(CompletionSink&&) = delete;
  CompletionSink& operator=(CompletionSink&&) = delete;
  virtual ~CompletionSink() = default;

  virtual void complete(ItemID id) = 0;
};

class Item {
public:
  Item(std::shared_ptr<CompletionSink> sink, ItemID id, std::string payload)
      : sink_(std::move(sink)), id_(id), payload_(std::move(payload)) {}

  // Item is non-copyable
  Item(const Item& other) = delete;
  Item& operator=(const Item& other) = delete;
  Item(Item&& other) = delete;
  Item& operator=(Item&& other) = delete;

  [[nodiscard]] ItemID id() const { return id_; }

  [[nodiscard]] const std::string& payload() const { return payload_; }

  ~Item() {
    try {
      sink_->complete(id_);
    } catch (...) {
      L_(info) << "Exception in Item::~Item() occurred";
    }
  }

private:
  const std::shared_ptr<CompletionSink> sink_;
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

/// State of a worker's connection to the distributor.
enum class ConnectionState {
  /// Not connected; a connection attempt is pending.
  Disconnected,
  /// A REGISTER message has been sent, waiting for the WELCOME reply.
  Registering,
  /// Registered, and the distributor has been heard from recently.
  Connected,
  /// Registered, but the distributor has missed at least one heartbeat.
  Degraded
};

inline std::string to_string(ConnectionState val) {
  switch (val) {
  case ConnectionState::Disconnected:
    return "disconnected";
  case ConnectionState::Registering:
    return "registering";
  case ConnectionState::Connected:
    return "connected";
  case ConnectionState::Degraded:
    return "degraded";
  }
  return "unknown";
}

struct WorkerParameters {
  /**
   * Request every item with sequence number n for which exists m in N:
   * n = m * stride + offset
   */
  size_t stride;
  size_t offset;
  WorkerQueuePolicy queue_policy;

  /**
   * Workers with the same group_id are treated as a group. The distributor will
   * only send items to one worker of the group. This is useful for load
   * sharing. Group_id 0 means no grouping.
   */
  size_t group_id;

  std::string client_name;

  /**
   * Number of items the distributor may have outstanding for this worker at
   * the same time. A window of 1 reproduces the strict one-item-at-a-time
   * behaviour; a larger window hides the round-trip time of the completion on
   * a high-latency connection, at the cost of buffering more items.
   */
  size_t window = 1;
};

#endif
