#include "ItemDistributor.hpp"
#include "ItemProducer.hpp"
#include "ItemWorkerProtocol.hpp"
#include "log.hpp"

#include <cassert>
#include <exception>
#include <memory>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>
#include <sstream>
#include <utility>
#include <zmq_addon.hpp>

// Handle incoming message (work item) from the generator
void ItemDistributor::on_generator_pollin() {
  zmq::multipart_t message(generator_socket_);

  // Receive item ID
  ItemID id = std::stoull(message.popstr());

  // Receive optional item payload
  std::string payload;
  if (!message.empty()) {
    payload = message.popstr();
  }

  auto new_item = std::make_shared<Item>(&completed_items_, id, payload);

  // Distribute the new work item.
  // If a group_id is set, send only once per group.
  std::set<size_t> completed_groups;
  for (auto& [identity, worker] : workers_) {
    if (worker->group_id() != 0 &&
        completed_groups.find(worker->group_id()) != completed_groups.end()) {
      // This group has already been served, skip it
      continue;
    }
    try {
      if (worker->wants(new_item->id())) {
        if (worker->queue_policy() == WorkerQueuePolicy::PrebufferOne) {
          worker->clear_queue();
        }
        if (worker->is_idle()) {
          // The worker is idle, send the item immediately
          if (worker->group_id() != 0) {
            completed_groups.insert(worker->group_id());
            // As we can send the item immediately, delete this work item from
            // the queues of other (previous) workers with the same group_id
            for (auto& [identity, other_worker] : workers_) {
              if (other_worker == worker) {
                break;
              }
              if (other_worker->group_id() == worker->group_id()) {
                other_worker->delete_from_queue(new_item->id());
              }
            }
          }
          worker->add_outstanding(new_item);
          send_worker_work_item(identity, *new_item);
        } else {
          // The worker is busy, enqueue the item
          if (worker->queue_policy() != WorkerQueuePolicy::Skip) {
            worker->push_queue(new_item);
          }
        }
      }
    } catch (std::exception& e) {
      L_(error) << e.what();
      workers_.erase(identity);
    }
  }
  new_item = nullptr;
  // A pending completion could occur here if this item is not sent to any
  // worker, so...
  send_pending_completions();
}

// Handle incoming message from a worker
void ItemDistributor::on_worker_pollin() {
  zmq::multipart_t message(worker_socket_);
  assert(message.size() >= 2);    // Multipart format ensured by ZMQ
  assert(!message.at(0).empty()); // for ROUTER sockets
  assert(message.at(1).empty());  //

  std::string identity = message.peekstr(0);

  if (message.size() == 2) {
    // Handle ZMQ worker disconnect notification
    if (workers_.count(identity) != 0) {
      L_(info) << "worker disconnected: "
               << workers_.at(identity)->description();
    }
    if (workers_.erase(identity) == 0) {
      // This could happen if a misbehaving worker did not send a REGISTER
      // message
      L_(error) << "disconnect from unknown worker";
    }
  } else {
    try {
      // Handle general message from a worker
      std::string message_string = message.peekstr(2);
      if (message_string.rfind("REGISTER ", 0) == 0) {
        // Handle new worker registration
        auto worker = std::make_unique<ItemDistributorWorker>(message_string);
        workers_[identity] = std::move(worker);
        L_(info) << "worker connected: "
                 << workers_.at(identity)->description();
      } else if (message_string.rfind("COMPLETE ", 0) == 0) {
        // Handle worker completion message
        auto& worker = workers_.at(identity);
        std::string command;
        ItemID id;
        std::stringstream s(message_string);
        s >> command >> id;
        if (s.fail()) {
          throw std::invalid_argument("Invalid completion message");
        }
        // Find the corresponding outstanding item object and delete it
        worker->delete_outstanding(id);
        // Send next item if available
        if (!worker->queue_empty()) {
          auto item = worker->pop_queue();
          if (worker->group_id() != 0) {
            // Delete this work item from the queues of other workers with the
            // same group_id
            for (auto& [identity, other_worker] : workers_) {
              if (worker != other_worker &&
                  other_worker->group_id() == worker->group_id()) {
                other_worker->delete_from_queue(item->id());
              }
            }
          }
          worker->add_outstanding(item);
          send_worker_work_item(identity, *item);
        } else {
          worker->reset_heartbeat_time();
        }
      } else if (message_string.rfind("HEARTBEAT", 0) == 0) {
        // Ignore heartbeat reply
      } else {
        throw std::invalid_argument("Unknown message type: " + message_string);
      }
    } catch (std::exception& e) {
      L_(error) << e.what();
      L_(error) << "protocol violation, disconnecting worker";
      try {
        send_worker_disconnect(identity);
      } catch (std::exception&) {
      };
      workers_.erase(identity);
    }
  }
  send_pending_completions();
}
