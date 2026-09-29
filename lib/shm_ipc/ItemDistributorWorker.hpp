#ifndef SHM_IPC_ITEMDISTRIBUTORWORKER_HPP
#define SHM_IPC_ITEMDISTRIBUTORWORKER_HPP

#include "ItemWorkerProtocol.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/// The distributor's view of a single connected worker.
class ItemDistributorWorker {
public:
  /**
   * Create a worker from the argument frames of its REGISTER message, i.e.
   * everything following the verb.
   */
  ItemDistributorWorker(const std::vector<std::string>& args,
                        size_t max_queued_items)
      : max_queued_items_(max_queued_items) {
    initialize_from_frames(args);
  }

  // ItemDistributorWorker is non-copyable
  ItemDistributorWorker(const ItemDistributorWorker& other) = delete;
  ItemDistributorWorker& operator=(const ItemDistributorWorker& other) = delete;
  ItemDistributorWorker(ItemDistributorWorker&& other) = delete;
  ItemDistributorWorker& operator=(ItemDistributorWorker&& other) = delete;
  ~ItemDistributorWorker() = default;

  [[nodiscard]] bool wants(ItemID id) const { return id % stride_ == offset_; }

  [[nodiscard]] WorkerQueuePolicy queue_policy() const { return queue_policy_; }

  [[nodiscard]] size_t group_id() const { return group_id_; }

  [[nodiscard]] size_t window() const { return window_; }

  [[nodiscard]] bool queue_empty() const { return waiting_items_.empty(); }

  void clear_queue() { waiting_items_.clear(); }

  void push_queue(const std::shared_ptr<Item>& item) {
    if (waiting_items_.size() >= max_queued_items_) {
      throw std::length_error("Item queue limit of " +
                              std::to_string(max_queued_items_) +
                              " exceeded for worker " + client_name_);
    }
    waiting_items_.push_back(item);
  }

  std::shared_ptr<Item> pop_queue() {
    if (queue_empty()) {
      return nullptr;
    }
    std::shared_ptr<Item> item = waiting_items_.front();
    waiting_items_.pop_front();
    return item;
  }

  void delete_from_queue(ItemID id) {
    auto it = std::find_if(
        std::begin(waiting_items_), std::end(waiting_items_),
        [id](const std::shared_ptr<Item>& i) { return i->id() == id; });
    if (it != std::end(waiting_items_)) {
      waiting_items_.erase(it);
    }
  }

  /// True if the distributor may send another item to this worker right now.
  [[nodiscard]] bool has_capacity() const {
    return outstanding_items_.size() < window_;
  }

  void add_outstanding(const std::shared_ptr<Item>& item) {
    outstanding_items_.push_back(item);
  }

  // Find an outstanding item object and delete it
  void delete_outstanding(ItemID id) {
    auto it = std::find_if(
        std::begin(outstanding_items_), std::end(outstanding_items_),
        [id](const std::shared_ptr<Item>& i) { return i->id() == id; });
    if (it == std::end(outstanding_items_)) {
      throw std::invalid_argument("Invalid work completion");
    }
    outstanding_items_.erase(it);
  }

  void reset_send_time() { last_send_time_ = std::chrono::steady_clock::now(); }

  void reset_recv_time() { last_recv_time_ = std::chrono::steady_clock::now(); }

  /// True if nothing has been sent to this worker for a heartbeat interval.
  [[nodiscard]] bool
  wants_heartbeat(std::chrono::steady_clock::time_point when) const {
    return last_send_time_ + heartbeat_interval < when;
  }

  /// True if nothing has been heard from this worker for the peer timeout.
  [[nodiscard]] bool
  is_expired(std::chrono::steady_clock::time_point when) const {
    return last_recv_time_ + peer_timeout < when;
  }

  /// True once END_OF_STREAM has been sent to this worker.
  [[nodiscard]] bool eos_sent() const { return eos_sent_; }

  void set_eos_sent() { eos_sent_ = true; }

  [[nodiscard]] const std::string& client_name() const { return client_name_; }

  [[nodiscard]] std::string description() const {
    return client_name_ + " (s" + std::to_string(stride_) + "/o" +
           std::to_string(offset_) + "/p" + to_string(queue_policy_) + "/g" +
           std::to_string(group_id_) + "/w" + std::to_string(window_) + ")";
  }

private:
  void initialize_from_frames(const std::vector<std::string>& args) {
    // version, stride, offset, queue_policy, group_id, window, client_name
    constexpr size_t num_args = 7;
    if (args.size() != num_args) {
      throw WorkerProtocolError("invalid register message: expected " +
                                std::to_string(num_args) + " arguments, got " +
                                std::to_string(args.size()));
    }
    const size_t version = parse_number_frame(args.at(0), "protocol version");
    if (version != item_protocol_version) {
      throw WorkerProtocolError("unsupported protocol version " +
                                std::to_string(version) +
                                ", this distributor speaks version " +
                                std::to_string(item_protocol_version));
    }
    stride_ = parse_number_frame(args.at(1), "stride");
    offset_ = parse_number_frame(args.at(2), "offset");
    queue_policy_ = static_cast<WorkerQueuePolicy>(
        parse_number_frame(args.at(3), "queue "
                                       "policy"));
    group_id_ = parse_number_frame(args.at(4), "group id");
    window_ = parse_number_frame(args.at(5), "window");
    client_name_ = args.at(6);

    // A zero stride would cause a division by zero in wants()
    if (stride_ == 0) {
      throw WorkerProtocolError("invalid register message: stride must not be "
                                "zero");
    }
    if (window_ == 0) {
      throw WorkerProtocolError("invalid register message: window must not be "
                                "zero");
    }
    if (queue_policy_ != WorkerQueuePolicy::QueueAll &&
        queue_policy_ != WorkerQueuePolicy::PrebufferOne &&
        queue_policy_ != WorkerQueuePolicy::Skip) {
      throw WorkerProtocolError("invalid register message: unknown queue "
                                "policy " +
                                args.at(3));
    }
    if (client_name_.empty()) {
      throw WorkerProtocolError("invalid register message: empty client name");
    }
  }

  const size_t max_queued_items_;

  size_t stride_{};
  size_t offset_{};
  WorkerQueuePolicy queue_policy_{};
  size_t group_id_{};
  size_t window_{};
  std::string client_name_;

  std::deque<std::shared_ptr<Item>> waiting_items_;
  std::deque<std::shared_ptr<Item>> outstanding_items_;
  bool eos_sent_ = false;
  std::chrono::steady_clock::time_point last_send_time_ =
      std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last_recv_time_ =
      std::chrono::steady_clock::now();
};

#endif
