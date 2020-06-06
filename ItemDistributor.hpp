#pragma once

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
#include <zmq.hpp>
#include <zmq_addon.hpp>

/*

Each worker ("ItemWorker") connects to the broker ("ItemDistributor").
It starts the communication by sending a REGISTER message. After that, the
broker can send a WORK_ITEM message at any time, and the worker can send a
COMPLETION message at any time. In addition, the broker sends HEARTBEAT messages
regularly if there is no outstanding WORK_ITEM. This allows the worker to detect
that the broker has died, in which case the worker closes the socket and opens a
new connection.

The broker keeps track of all connected workers. It detects that a worker has
died by the disconnect notification. When this happens, it releases all
outstanding WORK_ITEMs for that worker and stops sending messages to it.

The REGISTER message contains a specification of the type of items the worker
wants to receive (stride, offset). It also specifies the queueing mode:
- fully asynchronous, receive all, don't skip
  All matching items are immediately sent as a WORK_ITEM to the send queue, or,
if this fails, put on an internal waiting_items queue.
- prebuffer one
  The broker keeps the newest matching item in an internal 1-item queue if the
worker is not idle. The item is sent once the broker receives the outstanding
completion from the worker.
- skip
  The broker keeps no item queue for this worker. It only sends the item
immediately if the worker is idle.

Work items are received from an exclusive producer client through a ZMQ_PAIR
socket.

....
on new item n:
make new item object with shared_ptr and proper destructor (queue a message on
destruction) and item id member

  for all connections:

    if item "fits" connection:

      enqueue it

      if client is idle, send it immediately

on reception of a completion:
  remove item from outstanding_items

*/

// Request every item with sequence number n for which exists m in N:
// n = m * stride + offset

class Item {
public:
  Item(std::vector<ItemID>& completed_items,
       ItemID id,
       const std::string& payload)
      : completed_items_(completed_items), id_(id), payload_(payload) {}

  ItemID id() const { return id_; }

  const std::string& payload() const { return payload_; }

  ~Item() { completed_items_.emplace_back(id_); }

private:
  std::vector<ItemID>& completed_items_;
  const ItemID id_;
  const std::string payload_;
};

struct Worker {
  size_t stride;
  size_t offset;
  WorkerQueuePolicy queue_policy;
  std::string client_name;

  std::deque<std::shared_ptr<Item>> waiting_items;
  std::deque<std::shared_ptr<Item>> outstanding_items;
  // std::chrono::system_clock::time_point next_heartbeat_time;

  bool wants_item(ItemID id) const { return id % stride == offset; }
};

#define HEARTBEAT_INTERVAL 1000 // ms

class ItemDistributor {
public:
  ItemDistributor(const std::string& producer_address,
                  const std::string& worker_address) {
    generator_socket_.bind(producer_address);
    worker_socket_.set(zmq::sockopt::router_mandatory, 1);
    worker_socket_.set(zmq::sockopt::router_notify, ZMQ_NOTIFY_DISCONNECT);
    worker_socket_.bind(worker_address);
  }

  std::shared_ptr<Item> receive_producer_item() {
    ItemID id;
    std::string payload;

    zmq::message_t id_message;
    zmq::message_t payload_message;

    // Receive item ID
    const auto id_result = generator_socket_.recv(id_message);
    assert(id_result.has_value());
    id = std::stoull(id_message.to_string());

    // Receive optional item payload
    if (id_message.more()) {
      const auto payload_result = generator_socket_.recv(payload_message);
      assert(payload_result.has_value());
      payload = payload_message.to_string();
      assert(!payload_message.more());
    }

    return std::make_shared<Item>(completed_items_, id, payload);
  }

  void operator()() {
    while (true) {
      zmq::pollitem_t items[] = {
          {static_cast<void*>(generator_socket_), 0, ZMQ_POLLIN, 0},
          {static_cast<void*>(worker_socket_), 0, ZMQ_POLLIN, 0}};
      zmq::poll(items, 2, HEARTBEAT_INTERVAL);

      // Handle producer activity
      if (items[0].revents & ZMQ_POLLIN) {

        // Receive a new work item from the producer
        auto new_item = receive_producer_item();

        // Distribute the new work item
        for (auto& [identity, w] : workers_) {
          if (w->wants_item(new_item->id())) {
            if (w->queue_policy == WorkerQueuePolicy::PrebufferOne) {
              w->waiting_items.clear();
            }
            if (w->outstanding_items.empty()) {
              // The worker is idle, send the item immediately
              w->outstanding_items.push_back(new_item);
              send_work_item(identity, *new_item);
            } else {
              // The worker is busy, enqueue the item
              if (w->queue_policy != WorkerQueuePolicy::Skip) {
                w->waiting_items.push_back(new_item);
              }
            }
          }
        }
      }

      // Handle worker activity
      if (items[1].revents & ZMQ_POLLIN) {

        // Receive worker message
        zmq::multipart_t message(worker_socket_);
        assert(message.size() >= 2);       // Multipart format ensured by ZMQ
        assert(message.at(0).size() > 0);  // for ROUTER sockets
        assert(message.at(1).size() == 0); //

        std::string identity = message.peekstr(1);

        if (message.size() == 2) {

          // Handle ZMQ disconnect notification
          std::cout << "Info: received disconnect notification" << std::endl;
          if (workers_.erase(identity) != 0) {
            std::cerr << "Error: disconnect from unknown worker" << std::endl;
          }

        } else {

          std::string message_string = message.peekstr(3);
          if (message_string.rfind("REGISTER ", 0) == 0) {
            // Handle new worker registration
            auto c = std::make_unique<Worker>();
            std::string command;
            std::stringstream s(message_string);
            s >> command >> c->stride >> c->offset >> c->queue_policy >>
                c->client_name;
            assert(!s.fail());
            workers_[identity] = std::move(c);
          } else if (message_string.rfind("COMPLETE ", 0) == 0) {
            // Handle worker completion message
            auto& c = workers_.at(identity);
            std::string command;
            ItemID id;
            std::stringstream s(message_string);
            s >> command >> id;
            assert(!s.fail());
            // Find the corresponding outstanding item object and delete it
            auto it = std::find_if(
                std::begin(c->outstanding_items),
                std::end(c->outstanding_items),
                [id](const std::shared_ptr<Item>& i) { return i->id() == id; });
            assert(it != std::end(c->outstanding_items));
            c->outstanding_items.erase(it);
            // Send next item if available
            if (!c->waiting_items.empty()) {
              auto item = c->waiting_items.front();
              c->waiting_items.pop_front();
              c->outstanding_items.push_back(item);
              send_work_item(identity, *item);
            }
          }
        }
      }

      // send completions to generator
      try {
        for (auto item : completed_items_) {
          generator_socket_.send(zmq::buffer(std::to_string(item)));
        }
      } catch (zmq::error_t& error) {
        std::cout << "ERROR: " << error.what() << std::endl;
      }
      completed_items_.clear();
    }
  }

  void stop() {}

private:
  void send_work_item(const std::string& identity, const Item& item) {
    std::string message_str = "WORK_ITEM " + std::to_string(item.id());
    auto message = zmq::buffer(message_str);
    try {
      worker_socket_.send(zmq::buffer(identity), zmq::send_flags::sndmore);
      worker_socket_.send(zmq::message_t(), zmq::send_flags::sndmore);
      if (item.payload().empty()) {
        worker_socket_.send(message, zmq::send_flags::none);
      } else {
        worker_socket_.send(message, zmq::send_flags::sndmore);
        worker_socket_.send(zmq::buffer(item.payload()), zmq::send_flags::none);
      }
    } catch (zmq::error_t& error) {
      std::cout << "ERROR: " << error.what() << std::endl;
    }
  }

  void send_disconnect(const std::string& identity) {
    try {
      worker_socket_.send(zmq::buffer(identity), zmq::send_flags::sndmore);
      worker_socket_.send(zmq::message_t(), zmq::send_flags::sndmore);
      worker_socket_.send(zmq::buffer("DISCONNECT"));
    } catch (zmq::error_t& error) {
      std::cout << "ERROR: " << error.what() << std::endl;
    }
  }

  zmq::context_t context_{1};
  zmq::socket_t generator_socket_{context_, ZMQ_PAIR};
  zmq::socket_t worker_socket_{context_, ZMQ_ROUTER};
  std::vector<ItemID> completed_items_;
  std::map<std::string, std::unique_ptr<Worker>> workers_;
};
