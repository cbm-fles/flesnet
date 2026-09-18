// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_ItemProtocol
#include <boost/test/unit_test.hpp>

#include "ItemDistributor.hpp"
#include "ItemProducer.hpp"
#include "ItemWorker.hpp"
#include "ItemWorkerProtocol.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <zmq.hpp>
#include <zmq_addon.hpp>

using namespace std::chrono_literals;

namespace {

/// A distinct pair of addresses per test case, so that cases cannot collide.
struct Addresses {
  Addresses() {
    static int counter = 0;
    const std::string id = "item-protocol-test-" +
                           std::to_string(static_cast<int>(getpid())) + "-" +
                           std::to_string(counter++);
    producer = "inproc://" + id;
    worker = "ipc://@" + id;
  }
  std::string producer;
  std::string worker;
};

/// An ItemDistributor running on its own thread, stopped on destruction.
class RunningDistributor {
public:
  RunningDistributor(zmq::context_t& context,
                     const Addresses& addresses,
                     size_t max_queued_items = default_max_queued_items)
      : distributor_(std::make_unique<ItemDistributor>(
            context, addresses.producer, addresses.worker, max_queued_items)),
        thread_(std::ref(*distributor_)) {}

  ~RunningDistributor() { stop(); }

  void stop() {
    if (distributor_) {
      distributor_->stop();
      thread_.join();
      distributor_ = nullptr;
    }
  }

  ItemDistributor& get() { return *distributor_; }

private:
  std::unique_ptr<ItemDistributor> distributor_;
  std::thread thread_;
};

/**
 * Call worker.get() with an upper bound on the wait.
 *
 * Returns nullptr if nothing arrived in time, after stopping the worker so
 * that the helper always terminates rather than hanging the test run.
 */
std::shared_ptr<const Item>
try_get(ItemWorker& worker, std::chrono::milliseconds timeout = 5000ms) {
  std::promise<std::shared_ptr<const Item>> promise;
  auto future = promise.get_future();
  std::thread thread([&worker, &promise] { promise.set_value(worker.get()); });
  if (future.wait_for(timeout) != std::future_status::ready) {
    worker.stop();
  }
  thread.join();
  return future.get();
}

/// Drain the producer's completion channel, returning the ids it reports.
std::vector<ItemID> drain_completions(ItemProducer& producer,
                                      std::chrono::milliseconds duration) {
  std::vector<ItemID> ids;
  const auto deadline = std::chrono::steady_clock::now() + duration;
  while (std::chrono::steady_clock::now() < deadline) {
    ItemID id = 0;
    if (producer.try_receive_completion(&id)) {
      ids.push_back(id);
    } else {
      std::this_thread::sleep_for(5ms);
    }
  }
  return ids;
}

/// Wait until the worker has registered, so that no produced item is missed.
bool wait_until_connected(const ItemWorker& worker,
                          std::chrono::milliseconds timeout = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (worker.connection_state() == ConnectionState::Connected) {
      return true;
    }
    std::this_thread::sleep_for(5ms);
  }
  return false;
}

WorkerParameters params(const std::string& name,
                        size_t stride = 1,
                        size_t offset = 0,
                        WorkerQueuePolicy policy = WorkerQueuePolicy::QueueAll,
                        size_t group = 0,
                        size_t window = 1) {
  return WorkerParameters{stride, offset, policy, group, name, window};
}

/// A hand-built peer, for exercising the wire protocol directly.
class RawWorker {
public:
  explicit RawWorker(const std::string& address)
      : socket_(context_, zmq::socket_type::dealer) {
    socket_.set(zmq::sockopt::linger, 0);
    socket_.connect(address);
  }

  void send(const std::vector<std::string>& frames) {
    zmq::multipart_t message;
    for (const auto& frame : frames) {
      message.addstr(frame);
    }
    message.push(zmq::message_t(0));
    message.send(socket_);
  }

  /// Receive one message, without the delimiter frame. Empty on timeout.
  std::vector<std::string> receive(std::chrono::milliseconds timeout = 5000ms) {
    std::array<zmq_pollitem_t, 1> items = {
        {{socket_.handle(), 0, ZMQ_POLLIN, 0}}};
    zmq::poll(items.data(), items.size(), timeout);
    if ((items.at(0).revents & ZMQ_POLLIN) == 0) {
      return {};
    }
    zmq::multipart_t message;
    if (!message.recv(socket_)) {
      return {};
    }
    std::vector<std::string> frames;
    for (size_t i = 1; i < message.size(); ++i) {
      frames.push_back(message.peekstr(i));
    }
    return frames;
  }

  void disappear() { socket_.close(); }

private:
  zmq::context_t context_{1};
  zmq::socket_t socket_;
};

} // namespace

BOOST_AUTO_TEST_CASE(round_trip_and_end_of_stream) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  ItemProducer producer(context, addresses.producer);
  ItemWorker worker(addresses.worker, params("round_trip"));
  BOOST_REQUIRE(wait_until_connected(worker));

  constexpr size_t count = 10;
  for (size_t i = 0; i < count; ++i) {
    producer.send_work_item(i, "payload " + std::to_string(i));
  }
  producer.send_end_of_stream();

  for (size_t i = 0; i < count; ++i) {
    auto item = try_get(worker);
    BOOST_REQUIRE(item != nullptr);
    BOOST_CHECK_EQUAL(item->id(), i);
    BOOST_CHECK_EQUAL(item->payload(), "payload " + std::to_string(i));
  }

  // The end of the stream arrives only after the last item
  BOOST_CHECK(try_get(worker) == nullptr);
  BOOST_CHECK(worker.eos());

  // The distributor welcomed the worker and stayed connected throughout
  BOOST_CHECK(!worker.distributor_instance_id().empty());
  BOOST_CHECK_EQUAL(worker.reconnect_count(), 0);
  BOOST_CHECK_EQUAL(worker.items_received(), count);

  const auto completions = drain_completions(producer, 500ms);
  BOOST_CHECK_EQUAL(completions.size(), count);
}

// A consumer that holds an item while asking for the next one used to make the
// distributor stop sending heartbeats, so the worker declared the connection
// dead and reconnected in a loop. Liveness must not depend on work at all.
BOOST_AUTO_TEST_CASE(busy_worker_stays_connected) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  ItemProducer producer(context, addresses.producer);
  ItemWorker worker(addresses.worker, params("busy"));
  BOOST_REQUIRE(wait_until_connected(worker));

  producer.send_work_item(0, "");
  auto held = try_get(worker);
  BOOST_REQUIRE(held != nullptr);

  // Hold the item for well over the peer timeout without completing it
  std::this_thread::sleep_for(peer_timeout + heartbeat_interval);

  BOOST_CHECK_EQUAL(worker.reconnect_count(), 0);
  BOOST_CHECK(worker.connection_state() == ConnectionState::Connected);

  // Releasing it must still let the next item through
  producer.send_work_item(1, "");
  held = nullptr;
  auto next = try_get(worker);
  BOOST_REQUIRE(next != nullptr);
  BOOST_CHECK_EQUAL(next->id(), 1);
}

BOOST_AUTO_TEST_CASE(stride_and_offset_select_disjoint_items) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  ItemProducer producer(context, addresses.producer);
  ItemWorker even(addresses.worker, params("even", 2, 0));
  ItemWorker odd(addresses.worker, params("odd", 2, 1));
  BOOST_REQUIRE(wait_until_connected(even));
  BOOST_REQUIRE(wait_until_connected(odd));

  constexpr size_t count = 10;
  for (size_t i = 0; i < count; ++i) {
    producer.send_work_item(i, "");
  }

  for (size_t i = 0; i < count / 2; ++i) {
    auto item = try_get(even);
    BOOST_REQUIRE(item != nullptr);
    BOOST_CHECK_EQUAL(item->id() % 2, 0);
    item = try_get(odd);
    BOOST_REQUIRE(item != nullptr);
    BOOST_CHECK_EQUAL(item->id() % 2, 1);
  }
}

BOOST_AUTO_TEST_CASE(group_shares_the_load_without_duplicates) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  ItemProducer producer(context, addresses.producer);
  ItemWorker first(addresses.worker,
                   params("first", 1, 0, WorkerQueuePolicy::QueueAll, 7));
  ItemWorker second(addresses.worker,
                    params("second", 1, 0, WorkerQueuePolicy::QueueAll, 7));
  BOOST_REQUIRE(wait_until_connected(first));
  BOOST_REQUIRE(wait_until_connected(second));

  constexpr size_t count = 10;
  for (size_t i = 0; i < count; ++i) {
    producer.send_work_item(i, "");
  }

  std::set<ItemID> seen;
  for (size_t i = 0; i < count; ++i) {
    // Whichever worker is idle gets the item, so try both
    auto item = try_get(first, 200ms);
    if (item == nullptr) {
      item = try_get(second, 2000ms);
    }
    BOOST_REQUIRE(item != nullptr);
    BOOST_CHECK(seen.insert(item->id()).second);
  }
  BOOST_CHECK_EQUAL(seen.size(), count);
}

BOOST_AUTO_TEST_CASE(window_allows_several_outstanding_items) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  ItemProducer producer(context, addresses.producer);
  ItemWorker worker(
      addresses.worker,
      params("windowed", 1, 0, WorkerQueuePolicy::QueueAll, 0, 4));
  BOOST_REQUIRE(wait_until_connected(worker));

  for (size_t i = 0; i < 8; ++i) {
    producer.send_work_item(i, "");
  }

  // Four items can be held at once without completing any of them
  std::vector<std::shared_ptr<const Item>> held;
  for (size_t i = 0; i < 4; ++i) {
    auto item = try_get(worker);
    BOOST_REQUIRE(item != nullptr);
    held.push_back(item);
  }
  BOOST_CHECK_EQUAL(held.size(), 4);

  // Releasing them lets the rest follow
  held.clear();
  for (size_t i = 4; i < 8; ++i) {
    auto item = try_get(worker);
    BOOST_REQUIRE(item != nullptr);
    BOOST_CHECK_EQUAL(item->id(), i);
  }
}

BOOST_AUTO_TEST_CASE(protocol_version_mismatch_is_rejected) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  RawWorker raw(addresses.worker);

  raw.send({register_verb, std::to_string(item_protocol_version + 1), "1", "0",
            "0", "0", "1", "from_the_future"});

  const auto reply = raw.receive();
  BOOST_REQUIRE(!reply.empty());
  BOOST_CHECK_EQUAL(reply.at(0), disconnect_verb);
  BOOST_REQUIRE_EQUAL(reply.size(), 2);
  // The reason names both versions, so the mismatch is obvious in a log
  BOOST_CHECK(reply.at(1).find(std::to_string(item_protocol_version)) !=
              std::string::npos);
}

BOOST_AUTO_TEST_CASE(invalid_registration_is_rejected) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  RawWorker raw(addresses.worker);

  // A zero stride would divide by zero in the distributor
  raw.send({register_verb, std::to_string(item_protocol_version), "0", "0", "0",
            "0", "1", "zero_stride"});

  const auto reply = raw.receive();
  BOOST_REQUIRE(!reply.empty());
  BOOST_CHECK_EQUAL(reply.at(0), disconnect_verb);
}

BOOST_AUTO_TEST_CASE(welcome_precedes_the_first_item) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  ItemProducer producer(context, addresses.producer);
  RawWorker raw(addresses.worker);

  raw.send({register_verb, std::to_string(item_protocol_version), "1", "0", "0",
            "0", "1", "polite"});

  const auto welcome = raw.receive();
  BOOST_REQUIRE_EQUAL(welcome.size(), 5);
  BOOST_CHECK_EQUAL(welcome.at(0), welcome_verb);
  BOOST_CHECK_EQUAL(welcome.at(1), std::to_string(item_protocol_version));
  BOOST_CHECK_EQUAL(welcome.at(2), distributor.get().instance_id());

  producer.send_work_item(42, "");
  const auto work_item = raw.receive();
  BOOST_REQUIRE_EQUAL(work_item.size(), 2);
  BOOST_CHECK_EQUAL(work_item.at(0), work_item_verb);
  BOOST_CHECK_EQUAL(work_item.at(1), "42");
}

// Without the ROUTER disconnect notification, a worker that vanishes silently
// has to be detected from its missing heartbeats.
BOOST_AUTO_TEST_CASE(vanished_worker_releases_its_items) {
  zmq::context_t context{1};
  const Addresses addresses;
  RunningDistributor distributor(context, addresses);
  ItemProducer producer(context, addresses.producer);

  {
    RawWorker raw(addresses.worker);
    raw.send({register_verb, std::to_string(item_protocol_version), "1", "0",
              "0", "0", "1", "doomed"});
    BOOST_REQUIRE(!raw.receive().empty()); // welcome

    producer.send_work_item(0, "");
    const auto work_item = raw.receive();
    BOOST_REQUIRE_EQUAL(work_item.size(), 2);
    BOOST_CHECK_EQUAL(work_item.at(1), "0");

    // Disappear without completing the item and without saying goodbye
    raw.disappear();
  }

  const auto completions =
      drain_completions(producer, peer_timeout + 2 * heartbeat_interval);
  BOOST_REQUIRE_EQUAL(completions.size(), 1);
  BOOST_CHECK_EQUAL(completions.at(0), 0);
}

BOOST_AUTO_TEST_CASE(distributor_restart_is_visible_to_the_worker) {
  zmq::context_t context{1};
  const Addresses addresses;
  std::unique_ptr<ItemWorker> worker;
  std::string first_instance;

  // The first distributor gets its own context, whose termination waits
  // until the sockets are fully closed and the addresses are released, as
  // they are when a distributor process exits.
  {
    zmq::context_t first_context{1};
    RunningDistributor distributor(first_context, addresses);
    worker = std::make_unique<ItemWorker>(addresses.worker, params("survivor"));
    BOOST_REQUIRE(wait_until_connected(*worker));
    ItemProducer producer(first_context, addresses.producer);
    producer.send_work_item(0, "");
    auto item = try_get(*worker);
    BOOST_REQUIRE(item != nullptr);
    first_instance = worker->distributor_instance_id();
    BOOST_CHECK(!first_instance.empty());
  }

  // A second distributor takes over the same address
  RunningDistributor distributor(context, addresses);
  BOOST_REQUIRE(wait_until_connected(*worker, peer_timeout + 10000ms));
  ItemProducer producer(context, addresses.producer);
  producer.send_work_item(0, "");

  auto item = try_get(*worker);
  BOOST_REQUIRE(item != nullptr);
  BOOST_CHECK_EQUAL(item->id(), 0);
  BOOST_CHECK(worker->reconnect_count() > 0);
  BOOST_CHECK(worker->distributor_instance_id() != first_instance);
}

BOOST_AUTO_TEST_CASE(queue_limit_disconnects_a_stalled_worker) {
  zmq::context_t context{1};
  const Addresses addresses;
  constexpr size_t limit = 4;
  RunningDistributor distributor(context, addresses, limit);
  ItemProducer producer(context, addresses.producer);
  RawWorker raw(addresses.worker);

  raw.send({register_verb, std::to_string(item_protocol_version), "1", "0", "0",
            "0", "1", "stalled"});
  BOOST_REQUIRE(!raw.receive().empty()); // welcome

  // Never complete anything, so everything past the window piles up
  for (size_t i = 0; i < limit + 3; ++i) {
    producer.send_work_item(i, "");
  }

  bool disconnected = false;
  for (size_t i = 0; i < limit + 4 && !disconnected; ++i) {
    const auto message = raw.receive(2000ms);
    if (message.empty()) {
      break;
    }
    disconnected = message.at(0) == disconnect_verb;
  }
  BOOST_CHECK(disconnected);

  // The items it was holding are released back to the producer
  const auto completions = drain_completions(producer, 1000ms);
  BOOST_CHECK_EQUAL(completions.size(), limit + 3);
}
