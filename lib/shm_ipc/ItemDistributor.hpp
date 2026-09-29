#ifndef SHM_IPC_ITEMDISTRIBUTOR_HPP
#define SHM_IPC_ITEMDISTRIBUTOR_HPP

#include "ItemDistributorWorker.hpp"
#include "ItemWorkerProtocol.hpp"
#include "log.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include <zmq.hpp>
#include <zmq_addon.hpp>

/// Collects the completions of the items held by the distributor.
class DistributorCompletionSink : public CompletionSink {
public:
  void complete(ItemID id) override { completed_items_.push(id); }

  [[nodiscard]] bool empty() const { return completed_items_.empty(); }
  [[nodiscard]] ItemID front() const { return completed_items_.front(); }
  void pop() { completed_items_.pop(); }

private:
  std::queue<ItemID> completed_items_;
};

/**
 * Broker between a single producer and any number of workers.
 *
 * Work items are received from an exclusive producer client through a ZMQ_PAIR
 * socket and handed out to the workers connected to a ZMQ_ROUTER socket,
 * according to the selection and queueing parameters each worker registered
 * with. See ItemWorkerProtocol.hpp for the protocol.
 */
class ItemDistributor {
public:
  ItemDistributor(zmq::context_t& context,
                  const std::string& producer_address,
                  const std::string& worker_address,
                  size_t max_queued_items = default_max_queued_items)
      : max_queued_items_(max_queued_items),
        instance_id_(generate_instance_id()),
        generator_socket_(context, zmq::socket_type::pair),
        worker_socket_(context, zmq::socket_type::router) {
    generator_socket_.bind(producer_address);
    generator_socket_.set(zmq::sockopt::linger, 0);
    worker_socket_.set(zmq::sockopt::router_mandatory, 1);
    // Detect peers that vanish without closing the connection, which an
    // ipc:// socket reports immediately but a tcp:// one does not.
    worker_socket_.set(zmq::sockopt::heartbeat_ivl,
                       static_cast<int>(heartbeat_interval.count()));
    worker_socket_.set(zmq::sockopt::heartbeat_timeout,
                       static_cast<int>(peer_timeout.count()));
    worker_socket_.set(zmq::sockopt::tcp_keepalive, 1);
    worker_socket_.bind(worker_address);
    // Give a graceful DISCONNECT at shutdown a chance to reach the workers.
    worker_socket_.set(zmq::sockopt::linger,
                       static_cast<int>(heartbeat_interval.count() / 4));
  }

  // ItemDistributor is non-copyable
  ItemDistributor(const ItemDistributor& other) = delete;
  ItemDistributor& operator=(const ItemDistributor& other) = delete;
  ItemDistributor(ItemDistributor&& other) = delete;
  ItemDistributor& operator=(ItemDistributor&& other) = delete;

  void operator()() {
    std::array<zmq_pollitem_t, 2> items = {{
        {generator_socket_.handle(), 0, ZMQ_POLLIN, 0},
        {worker_socket_.handle(), 0, ZMQ_POLLIN, 0},
    }};

    while (!stopped_) {
      try {
        zmq::poll(items.data(), items.size(), poll_timeout);
      } catch (const zmq::error_t& e) {
        if (e.num() == EINTR) {
          continue;
        }
        throw;
      }
      if ((items.at(0).revents & ZMQ_POLLIN) != 0) {
        on_generator_pollin();
      }
      if ((items.at(1).revents & ZMQ_POLLIN) != 0) {
        on_worker_pollin();
      }
      remove_expired_workers();
      send_pending_end_of_stream();
      send_heartbeats();
    }
    disconnect_all_workers();
  }

  void stop() { stopped_ = true; }

  /// Identifier of this distributor instance, announced in WELCOME.
  [[nodiscard]] const std::string& instance_id() const { return instance_id_; }

  ~ItemDistributor() = default;

private:
  static std::string generate_instance_id();

  // Remove workers that have not been heard from for the peer timeout
  void remove_expired_workers() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::string> dead_workers;
    for (auto& [identity, worker] : workers_) {
      if (worker->is_expired(now)) {
        L_(warning) << "worker timed out: " << worker->description();
        dead_workers.push_back(identity);
      }
    }
    remove_workers(dead_workers);
  }

  // Send a heartbeat to workers that have not been sent anything for a while
  void send_heartbeats() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::string> dead_workers;
    for (auto& [identity, worker] : workers_) {
      try {
        if (worker->wants_heartbeat(now)) {
          send_worker_heartbeat(identity);
        }
      } catch (std::exception& e) {
        L_(warning) << "cannot reach worker " << worker->description() << ": "
                    << e.what();
        dead_workers.push_back(identity);
      }
    }
    remove_workers(dead_workers);
  }

  // Tell every worker that has received all its items that none will follow
  void send_pending_end_of_stream() {
    if (!end_of_stream_) {
      return;
    }
    std::vector<std::string> dead_workers;
    for (auto& [identity, worker] : workers_) {
      if (worker->eos_sent() || !worker->queue_empty()) {
        continue;
      }
      try {
        send_worker_end_of_stream(identity);
        worker->set_eos_sent();
      } catch (std::exception& e) {
        L_(warning) << "cannot reach worker " << worker->description() << ": "
                    << e.what();
        dead_workers.push_back(identity);
      }
    }
    remove_workers(dead_workers);
  }

  void disconnect_all_workers() {
    for (auto& [identity, worker] : workers_) {
      try {
        send_worker_disconnect(identity, "distributor is shutting down");
      } catch (std::exception&) {
      }
    }
    workers_.clear();
    send_pending_completions();
  }

  /**
   * Erase the given workers and forward the completions this releases.
   *
   * Erasing from workers_ while iterating over it would invalidate the loop
   * iterator, so every caller collects the identities first.
   */
  void remove_workers(const std::vector<std::string>& identities) {
    if (identities.empty()) {
      return;
    }
    for (const auto& identity : identities) {
      workers_.erase(identity);
    }
    send_pending_completions();
  }

  void send_pending_completions() {
    while (!completion_sink_->empty()) {
      // A PAIR socket blocks indefinitely once its peer is gone, so never send
      // blindly. Deferring is always safe: the completion stays queued and is
      // retried on the next pass through the event loop.
      if ((generator_socket_.get(zmq::sockopt::events) & ZMQ_POLLOUT) == 0) {
        return;
      }
      zmq::multipart_t message;
      message.addstr(complete_verb);
      message.addstr(std::to_string(completion_sink_->front()));
      message.send(generator_socket_);
      completion_sink_->pop();
    }
  }

  // Handle incoming message (work item) from the generator
  void on_generator_pollin();

  // Handle incoming message from a worker
  void on_worker_pollin();

  // Hand an item to a worker, or queue it, according to the worker's policy
  void offer_item(const std::string& identity,
                  ItemDistributorWorker& worker,
                  const std::shared_ptr<Item>& item,
                  std::set<size_t>& served_groups);

  // Retire a completed item and send whatever the window now allows
  void handle_completion(const std::string& identity,
                         ItemDistributorWorker& worker,
                         ItemID id);

  void send_worker(const std::string& identity, zmq::multipart_t&& message) {
    assert(!identity.empty());
    // Prepare first two message parts as required for a ROUTER socket
    message.push(zmq::message_t(0));
    message.pushstr(identity);

    // Send the message
    if (!message.send(worker_socket_)) {
      L_(error) << "message send failed";
    }
    auto it = workers_.find(identity);
    if (it != workers_.end()) {
      it->second->reset_send_time();
    }
  }

  void send_worker_welcome(const std::string& identity) {
    zmq::multipart_t message;
    message.addstr(welcome_verb);
    message.addstr(std::to_string(item_protocol_version));
    message.addstr(instance_id_);
    message.addstr(std::to_string(heartbeat_interval.count()));
    message.addstr(std::to_string(heartbeat_liveness));
    send_worker(identity, std::move(message));
  }

  void send_worker_work_item(const std::string& identity, const Item& item) {
    zmq::multipart_t message;
    message.addstr(work_item_verb);
    message.addstr(std::to_string(item.id()));
    if (!item.payload().empty()) {
      message.addstr(item.payload());
    }
    send_worker(identity, std::move(message));
  }

  void send_worker_heartbeat(const std::string& identity) {
    zmq::multipart_t message;
    message.addstr(heartbeat_verb);
    send_worker(identity, std::move(message));
  }

  void send_worker_end_of_stream(const std::string& identity) {
    zmq::multipart_t message;
    message.addstr(end_of_stream_verb);
    send_worker(identity, std::move(message));
  }

  void send_worker_disconnect(const std::string& identity,
                              const std::string& reason) {
    zmq::multipart_t message;
    message.addstr(disconnect_verb);
    message.addstr(reason);
    send_worker(identity, std::move(message));
  }

  const size_t max_queued_items_;
  const std::string instance_id_;
  // Declared before workers_, so that it outlives the items they hold
  const std::shared_ptr<DistributorCompletionSink> completion_sink_ =
      std::make_shared<DistributorCompletionSink>();
  zmq::socket_t generator_socket_;
  zmq::socket_t worker_socket_;
  std::map<std::string, std::unique_ptr<ItemDistributorWorker>> workers_;
  bool end_of_stream_ = false;
  std::atomic<bool> stopped_ = false;
};

#endif
