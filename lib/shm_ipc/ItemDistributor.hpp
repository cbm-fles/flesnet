#ifndef SHM_IPC_ITEMDISTRIBUTOR_HPP
#define SHM_IPC_ITEMDISTRIBUTOR_HPP

#include "ItemDistributorWorker.hpp"
#include "ItemWorkerProtocol.hpp"
#include "log.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <exception>
#include <map>
#include <memory>
#include <queue>
#include <string>

#include <zmq.hpp>
#include <zmq_addon.hpp>

/**
 * Work items are received from an exclusive producer client through a ZMQ_PAIR
 * socket.
 */
class ItemDistributor {
public:
  ItemDistributor(zmq::context_t& context,
                  const std::string& producer_address,
                  const std::string& worker_address)
      : generator_socket_(context, zmq::socket_type::pair),
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
        L_(error) << e.what();
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
      L_(error) << "message send failed";
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

  zmq::socket_t generator_socket_;
  zmq::socket_t worker_socket_;
  std::queue<ItemID> completed_items_;
  std::map<std::string, std::unique_ptr<ItemDistributorWorker>> workers_;
  bool stopped_ = false;
};

#endif
