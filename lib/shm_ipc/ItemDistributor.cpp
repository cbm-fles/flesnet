#include "ItemDistributor.hpp"

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

  // Distribute the new work item
  for (auto& [identity, worker] : workers_) {
    try {
      if (worker->wants(new_item->id())) {
        if (worker->queue_policy() == WorkerQueuePolicy::PrebufferOne) {
          worker->clear_queue();
        }
        if (worker->is_idle()) {
          // The worker is idle, send the item immediately
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
      std::cerr << "Error: " << e.what() << std::endl;
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
    std::cout << "Info: received disconnect notification" << std::endl;
    if (workers_.erase(identity) == 0) {
      // This could happen if a misbehaving worker did not send a REGISTER
      // message
      std::cerr << "Error: disconnect from unknown worker" << std::endl;
    }
  } else {
    try {
      // Handle general message from a worker
      std::string message_string = message.peekstr(2);
      if (message_string.rfind("REGISTER ", 0) == 0) {
        // Handle new worker registration
        auto worker = std::make_unique<Worker>(message_string);
        workers_[identity] = std::move(worker);
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
      std::cerr << e.what() << "\n"
                << "Error: protocol violation, disconnecting worker"
                << std::endl;
      try {
        send_worker_disconnect(identity);
      } catch (std::exception&) {
      };
      workers_.erase(identity);
    }
  }
  send_pending_completions();
}
