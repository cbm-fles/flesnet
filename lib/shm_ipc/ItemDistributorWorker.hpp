#ifndef SHM_IPC_ITEMDISTRIBUTORWORKER_HPP
#define SHM_IPC_ITEMDISTRIBUTORWORKER_HPP

#include "ItemWorkerProtocol.hpp"

#include <algorithm>
#include <memory>
#include <sstream>

class ItemDistributorWorker {
public:
  explicit ItemDistributorWorker(const std::string& message) {
    initialize_from_string(message);
  }

  // ItemDistributorWorker is non-copyable
  ItemDistributorWorker(const ItemDistributorWorker& other) = delete;
  ItemDistributorWorker& operator=(const ItemDistributorWorker& other) = delete;
  ItemDistributorWorker(ItemDistributorWorker&& other) = delete;
  ItemDistributorWorker& operator=(ItemDistributorWorker&& other) = delete;
  ~ItemDistributorWorker() = default;

  [[nodiscard]] bool wants(ItemID id) const { return id % stride_ == offset_; }

  [[nodiscard]] WorkerQueuePolicy queue_policy() const { return queue_policy_; }

  [[nodiscard]] bool queue_empty() const { return waiting_items_.empty(); }

  void clear_queue() { waiting_items_.clear(); }

  void push_queue(const std::shared_ptr<Item>& item) {
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

  [[nodiscard]] bool is_idle() const { return outstanding_items_.empty(); }

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

  void reset_heartbeat_time() {
    last_heartbeat_time_ = std::chrono::system_clock::now();
  }

  [[nodiscard]] bool
  wants_heartbeat(std::chrono::system_clock::time_point when) const {
    return (is_idle() &&
            last_heartbeat_time_ + distributor_heartbeat_interval < when);
  }

private:
  void initialize_from_string(const std::string& message) {
    std::string command;
    std::stringstream s(message);
    s >> command >> stride_ >> offset_ >> queue_policy_ >> client_name_;
    if (s.fail()) {
      throw std::invalid_argument("Invalid register message: " + message);
    }
  }

  size_t stride_{};
  size_t offset_{};
  WorkerQueuePolicy queue_policy_{};
  std::string client_name_;

  std::deque<std::shared_ptr<Item>> waiting_items_;
  std::deque<std::shared_ptr<Item>> outstanding_items_;
  std::chrono::system_clock::time_point last_heartbeat_time_ =
      std::chrono::system_clock::now();
};

#endif
