#ifndef SHM_IPC_ITEMDISTRIBUTOR_HPP
#define SHM_IPC_ITEMDISTRIBUTOR_HPP

#include "ItemWorkerProtocol.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <deque>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

class Worker {
public:
  explicit Worker(const std::string& message) {
    initialize_from_string(message);
  }

  // Worker is non-copyable
  Worker(const Worker& other) = delete;
  Worker& operator=(const Worker& other) = delete;
  Worker(Worker&& other) = delete;
  Worker& operator=(Worker&& other) = delete;
  ~Worker() = default;

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

/**
 * Work items are received from an exclusive producer client through a ZMQ_PAIR
 * socket.
 */
class ItemDistributor {
public:
  ItemDistributor(zmq::context_t& context,
                  const std::string& producer_address,
                  const std::string& worker_address)
      : context_(context), generator_socket_(context, zmq::socket_type::pair),
        worker_socket_(context, zmq::socket_type::router) {
    generator_socket_.bind(producer_address);
    generator_socket_.set(zmq::sockopt::linger, 0);
    worker_socket_.set(zmq::sockopt::router_mandatory, 1);
    worker_socket_.set(zmq::sockopt::router_notify, ZMQ_NOTIFY_DISCONNECT);
    worker_socket_.bind(worker_address);
    worker_socket_.set(zmq::sockopt::linger, 0);
  }

  // ItemDistributor is non-copyable
  ItemDistributor(const ItemDistributor& other) = delete;
  ItemDistributor& operator=(const ItemDistributor& other) = delete;
  ItemDistributor(ItemDistributor&& other) = delete;
  ItemDistributor& operator=(ItemDistributor&& other) = delete;

  void operator()() {
    zmq::active_poller_t poller;
    poller.add(generator_socket_, zmq::event_flags::pollin,
               [&](zmq::event_flags /*e*/) { on_generator_pollin(); });
    poller.add(worker_socket_, zmq::event_flags::pollin,
               [&](zmq::event_flags /*e*/) { on_worker_pollin(); });

    while (!stopped_) {
      poller.wait(distributor_poll_timeout);
      send_heartbeats();
    }
  }

  void stop() { stopped_ = true; }

  // TODO(cuveland): sensible clean-up
  ~ItemDistributor() = default;

private:
  // Send heartbeat messages to workers that have been idle for a while
  void send_heartbeats() {
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    for (auto& [identity, worker] : workers_) {
      try {
        if (worker->wants_heartbeat(now)) {
          worker->reset_heartbeat_time();
          send_worker_heartbeat(identity);
        }
      } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        workers_.erase(identity);
      }
    }
  }

  void send_pending_completions() {
    while (!completed_items_.empty()) {
      auto item = completed_items_.front();
      generator_socket_.send(zmq::buffer(std::to_string(item)));
      completed_items_.pop();
    }
  }

  // Handle incoming message (work item) from the generator
  void on_generator_pollin();

  // Handle incoming message from a worker
  void on_worker_pollin();

  void send_worker(const std::string& identity, zmq::multipart_t&& message) {
    assert(!identity.empty());
    // Prepare first two message parts as required for a ROUTER socket
    message.push(zmq::message_t(0));
    message.pushstr(identity);

    // Send the message
    if (!message.send(worker_socket_)) {
      std::cerr << "Error: message send failed";
    }
  }

  void send_worker_work_item(const std::string& identity, const Item& item) {
    zmq::multipart_t message("WORK_ITEM " + std::to_string(item.id()));
    if (!item.payload().empty()) {
      message.addstr(item.payload());
    }
    send_worker(identity, std::move(message));
  }

  void send_worker_heartbeat(const std::string& identity) {
    zmq::multipart_t message("HEARTBEAT");
    send_worker(identity, std::move(message));
  }

  void send_worker_disconnect(const std::string& identity) {
    zmq::multipart_t message("DISCONNECT");
    send_worker(identity, std::move(message));
  }

  zmq::context_t& context_;
  zmq::socket_t generator_socket_;
  zmq::socket_t worker_socket_;
  std::queue<ItemID> completed_items_;
  std::map<std::string, std::unique_ptr<Worker>> workers_;
  bool stopped_ = false;
};

#endif
