#ifndef ZMQ_DEMO_ITEMWORKER_HPP
#define ZMQ_DEMO_ITEMWORKER_HPP

#include "ItemWorkerProtocol.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include <zmq.hpp>

class ItemWorker {
public:
  constexpr static auto average_wait_time_ = std::chrono::milliseconds{500};

  explicit ItemWorker(const std::string& distributor_address) {
    distributor_socket_.connect(distributor_address);
  };

  static void do_work(ItemID /*id*/, const std::string& /*payload*/) {
    static std::default_random_engine eng{std::random_device{}()};
    static std::exponential_distribution<> dist(
        std::chrono::duration<double>(average_wait_time_).count());

    // Wait for a random time before completion
    std::this_thread::sleep_for(std::chrono::duration<double>{dist(eng)});
  }

  void operator()() {
    // send REGISTER
    auto str = register_str(parameters_);
    distributor_socket_.send(zmq::buffer(str));
    std::cout << "Worker sent " << str << std::endl;

    while (true) {

      // receive message
      zmq::multipart_t message{distributor_socket_};
      auto message_string = message.popstr();
      ItemID id{};
      std::string payload;

      if (is_work_item(message_string)) {
        // Handle new work item
        std::string command;
        std::stringstream s(message_string);
        s >> command >> id;
        assert(!s.fail());
      } else if (is_heartbeat(message_string)) {
        // send HEARTBEAT ACKNOWLEDGE
        try {
          distributor_socket_.send(zmq::buffer("HEARTBEAT"));
        } catch (zmq::error_t& error) {
          std::cerr << "ERROR: " << error.what() << std::endl;
        }
        continue;
      } else if (is_disconnect(message_string)) {
        // TODO: disconnect
      } else {
        std::cerr << "Error: This should not happen" << std::endl;
      }
      std::cout << "Worker received work item " << id << std::endl;
      if (!message.empty()) {
        payload = message.popstr();
      }
      do_work(id, payload);

      // send COMPLETE
      std::string complete_str = "COMPLETE " + std::to_string(id);
      try {
        distributor_socket_.send(zmq::buffer(complete_str));
        std::cout << "Worker sent " << complete_str << std::endl;
      } catch (zmq::error_t& error) {
        std::cerr << "ERROR: " << error.what() << std::endl;
      }
    }
  }

private:
  zmq::context_t context_{1};
  zmq::socket_t distributor_socket_{context_, zmq::socket_type::req};

  const WorkerParameters parameters_{1, 0, WorkerQueuePolicy::QueueAll,
                                     "example_client"};
};

#endif
