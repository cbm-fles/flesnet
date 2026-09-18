#include "ItemDistributor.hpp"
#include "ItemWorkerProtocol.hpp"
#include "log.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <zmq_addon.hpp>

std::string ItemDistributor::generate_instance_id() {
  std::random_device random_device;
  std::mt19937_64 generator(random_device());
  std::uniform_int_distribution<uint64_t> distribution;
  std::ostringstream stream;
  stream << std::hex << std::setw(16) << std::setfill('0')
         << distribution(generator);
  return stream.str();
}

// Handle incoming message (work item) from the generator
void ItemDistributor::on_generator_pollin() {
  zmq::multipart_t message(generator_socket_);
  if (message.empty()) {
    throw WorkerProtocolError("empty message from the producer");
  }
  const std::string verb = message.popstr();

  if (verb == end_of_stream_verb) {
    end_of_stream_ = true;
    return;
  }
  if (verb != work_item_verb) {
    throw WorkerProtocolError("unknown message from the producer: " + verb);
  }
  if (message.empty()) {
    throw WorkerProtocolError("work item without an id");
  }

  // Receive item ID and optional item payload
  const ItemID id = parse_number_frame(message.popstr(), "item id");
  std::string payload;
  if (!message.empty()) {
    payload = message.popstr();
  }

  auto new_item =
      std::make_shared<Item>(completion_sink_, id, std::move(payload));

  // Distribute the new work item.
  // If a group_id is set, send only once per group.
  std::set<size_t> served_groups;
  std::vector<std::string> dead_workers;
  for (auto& [identity, worker] : workers_) {
    if (worker->group_id() != 0 &&
        served_groups.find(worker->group_id()) != served_groups.end()) {
      // This group has already been served, skip it
      continue;
    }
    try {
      offer_item(identity, *worker, new_item, served_groups);
    } catch (std::exception& e) {
      L_(warning) << "dropping worker " << worker->description() << ": "
                  << e.what();
      try {
        send_worker_disconnect(identity, e.what());
      } catch (std::exception&) {
      }
      dead_workers.push_back(identity);
    }
  }
  new_item = nullptr;
  // A pending completion could occur here if this item is not sent to any
  // worker, so...
  remove_workers(dead_workers);
  send_pending_completions();
}

void ItemDistributor::offer_item(const std::string& identity,
                                 ItemDistributorWorker& worker,
                                 const std::shared_ptr<Item>& item,
                                 std::set<size_t>& served_groups) {
  if (!worker.wants(item->id())) {
    return;
  }
  if (worker.queue_policy() == WorkerQueuePolicy::PrebufferOne) {
    worker.clear_queue();
  }
  if (worker.has_capacity()) {
    // The worker has room for another item, send it immediately
    if (worker.group_id() != 0) {
      served_groups.insert(worker.group_id());
      // As we can send the item immediately, delete this work item from
      // the queues of other (previous) workers with the same group_id
      for (auto& [other_identity, other_worker] : workers_) {
        if (other_worker.get() == &worker) {
          break;
        }
        if (other_worker->group_id() == worker.group_id()) {
          other_worker->delete_from_queue(item->id());
        }
      }
    }
    worker.add_outstanding(item);
    send_worker_work_item(identity, *item);
  } else if (worker.queue_policy() != WorkerQueuePolicy::Skip) {
    // The worker is busy, enqueue the item
    worker.push_queue(item);
  }
}

void ItemDistributor::handle_completion(const std::string& identity,
                                        ItemDistributorWorker& worker,
                                        ItemID id) {
  // Find the corresponding outstanding item object and delete it
  worker.delete_outstanding(id);
  // Send further items while the worker's window allows it
  while (worker.has_capacity() && !worker.queue_empty()) {
    auto item = worker.pop_queue();
    if (worker.group_id() != 0) {
      // Delete this work item from the queues of other workers with the
      // same group_id
      for (auto& [other_identity, other_worker] : workers_) {
        if (other_worker.get() != &worker &&
            other_worker->group_id() == worker.group_id()) {
          other_worker->delete_from_queue(item->id());
        }
      }
    }
    worker.add_outstanding(item);
    send_worker_work_item(identity, *item);
  }
}

// Handle incoming message from a worker
void ItemDistributor::on_worker_pollin() {
  zmq::multipart_t message(worker_socket_);
  assert(message.size() >= 2);    // Multipart format ensured by ZMQ
  assert(!message.at(0).empty()); // for ROUTER sockets
  assert(message.at(1).empty());  //

  const std::string identity = message.peekstr(0);

  if (message.size() < 3) {
    L_(error) << "malformed message from a worker";
    return;
  }

  const std::string verb = message.peekstr(2);
  std::vector<std::string> args;
  args.reserve(message.size() - 3);
  for (size_t i = 3; i < message.size(); ++i) {
    args.push_back(message.peekstr(i));
  }

  try {
    if (verb == register_verb) {
      // Handle new worker registration. An existing entry for this identity is
      // replaced, which releases the items outstanding for it.
      auto worker =
          std::make_unique<ItemDistributorWorker>(args, max_queued_items_);
      L_(info) << "worker connected: " << worker->description();
      workers_[identity] = std::move(worker);
      send_worker_welcome(identity);
      workers_.at(identity)->reset_recv_time();
    } else {
      auto it = workers_.find(identity);
      if (it == workers_.end()) {
        throw WorkerProtocolError("message from an unregistered worker: " +
                                  verb);
      }
      auto& worker = *it->second;
      worker.reset_recv_time();

      if (verb == complete_verb) {
        if (args.empty()) {
          throw WorkerProtocolError("completion message without an item id");
        }
        for (const auto& arg : args) {
          handle_completion(identity, worker,
                            parse_number_frame(arg, "item "
                                                    "id"));
        }
      } else if (verb == heartbeat_verb) {
        // Nothing to do, the receive time has been updated already
      } else if (verb == disconnect_verb) {
        L_(info) << "worker disconnected: " << worker.description()
                 << (args.empty() ? "" : " (" + args.at(0) + ")");
        remove_workers({identity});
      } else {
        throw WorkerProtocolError("unknown message type: " + verb);
      }
    }
  } catch (std::exception& e) {
    L_(error) << "protocol violation, disconnecting worker: " << e.what();
    try {
      send_worker_disconnect(identity, e.what());
    } catch (std::exception&) {
    }
    remove_workers({identity});
  }
  send_pending_completions();
}
