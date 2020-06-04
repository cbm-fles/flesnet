#pragma once

#include <chrono>
#include <iostream>
#include <thread>
#include <zmq.hpp>

class MemoryItemServer {
public:
  MemoryItemServer(const std::string& address) {
    socket_.set(zmq::sockopt::router_mandatory, 1);
    socket_.set(zmq::sockopt::router_notify,
                ZMQ_NOTIFY_CONNECT | ZMQ_NOTIFY_DISCONNECT);
    socket_.bind(address);
  }

  void put(const std::string& item) {
    // ...
  }

  void operator()() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    zmq::message_t identifier;
    zmq::message_t separator;
    zmq::message_t message;

    {
      // connect message
      {
        zmq::message_t identifier;
        zmq::message_t separator;

        auto result = socket_.recv(identifier);
        assert(result.value_or(0ULL) > 0ULL);
        assert(identifier.more());

        result = socket_.recv(separator);
        assert(result.has_value());
        assert(separator.size() == 0LL);
        assert(!separator.more());
        std::cout << "received connect message" << std::endl;
      }

      auto result = socket_.recv(identifier);
      assert(result.value_or(0ULL) > 0ULL);
      assert(identifier.more());

      result = socket_.recv(separator);
      assert(result.has_value());
      assert(separator.size() == 0LL);
      assert(separator.more());

      result = socket_.recv(message);
      assert(result.has_value());

      std::string request =
          std::string(static_cast<char*>(message.data()), message.size());
      std::cout << "server received " << message.size() << ": " << request
                << std::endl;

      for (size_t i = 0; i < identifier.size(); ++i) {
        std::cout << i << ": "
                  << static_cast<int>(static_cast<char*>(identifier.data())[i])
                  << " "
                  << static_cast<char>(static_cast<char*>(identifier.data())[i])
                  << std::endl;
      }
    }

    {
      const std::string item = "Hi!";
      zmq::message_t message(item.size());
      std::copy_n(static_cast<const char*>(item.data()), message.size(),
                  static_cast<char*>(message.data()));

      try {
        socket_.send(identifier, zmq::send_flags::sndmore);
        socket_.send(separator, zmq::send_flags::sndmore);
        socket_.send(message, zmq::send_flags::none);
        std::cout << "server sent: " << item << std::endl;
      } catch (zmq::error_t& error) {
        std::cout << "ERROR: " << error.what() << std::endl;
      }
    }
    // disconnect message
    {
      auto result = socket_.recv(identifier);
      assert(result.value_or(0ULL) > 0ULL);
      assert(identifier.more());

      result = socket_.recv(separator);
      assert(result.has_value());
      assert(separator.size() == 0LL);
      assert(!separator.more());
    }
    std::cout << "received disconnect message" << std::endl;
  }

  void stop() {
  }

private:
  zmq::context_t context_{1};
  zmq::socket_t socket_{context_, ZMQ_ROUTER};
  std::unique_ptr<std::thread> monitor_thread_;
};
