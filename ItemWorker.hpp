#ifndef ZMQ_DEMO_ITEMWORKER_HPP
#define ZMQ_DEMO_ITEMWORKER_HPP

#include "ItemWorkerProtocol.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include <zmq.hpp>

class ItemWorker {
public:
  constexpr static auto wait_time_ = std::chrono::milliseconds{500};

  explicit ItemWorker(const std::string& distributor_address) {
    distributor_socket_.connect(distributor_address);
  };

  static void do_work(ItemID /*id*/, const std::string& /*payload*/) {
    std::this_thread::sleep_for(wait_time_);
  }

  void operator()() {
    // send REGISTER
    std::string register_str = "REGISTER " + std::to_string(stride_) + " " +
                              std::to_string(offset_) + " " +
                              to_string(queue_policy_) + " " + client_name_;
    distributor_socket_.send(zmq::buffer(register_str));
    std::cout << "Worker send: " << register_str << std::endl;

    while (true) {

      // receive message
      ItemID id{};
      std::string payload;
      zmq::message_t message;
      const auto result = distributor_socket_.recv(message);
      assert(result.has_value());
      std::string message_string = message.to_string();
      if (message_string.rfind("WORK_ITEM ", 0) == 0) {
        // Handle new work item
        std::string command;
        std::stringstream s(message_string);
        s >> command >> id;
        assert(!s.fail());
      } else if (message_string.rfind("HEARTBEAT", 0) == 0) {
        std::cout << "Worker received heartbeat message" << std::endl;
        // send HEARTBEAT ACKNOWLEDGE
        try {
          distributor_socket_.send(zmq::buffer("HEARTBEAT"));
          std::cout << "Worker send: " << message_string << std::endl;
        } catch (zmq::error_t& error) {
          std::cerr << "ERROR: " << error.what() << std::endl;
        }
        continue;
      } else {
        std::cerr << "this should not happen" << std::endl;
      }
      std::cout << "received work item " << id << std::endl;
      if (message.more()) {
        zmq::message_t payload_message;
        const auto result = distributor_socket_.recv(payload_message);
        assert(result.has_value());
        payload = payload_message.to_string();
      }
      do_work(id, payload);

      // send COMPLETE
      std::string complete_str = "COMPLETE " + std::to_string(id);
      try {
        distributor_socket_.send(zmq::buffer(complete_str));
        std::cout << "Worker send: " << complete_str << std::endl;
      } catch (zmq::error_t& error) {
        std::cerr << "ERROR: " << error.what() << std::endl;
      }
    }
  }

private:
  zmq::context_t context_{1};
  zmq::socket_t distributor_socket_{context_, zmq::socket_type::req};

  const size_t stride_ = 1;
  const size_t offset_ = 0;
  const WorkerQueuePolicy queue_policy_ = WorkerQueuePolicy::QueueAll;
  const std::string client_name_ = "example_client";
};

#endif
