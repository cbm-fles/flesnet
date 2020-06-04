#pragma once

#include <iostream>
#include <zmq.hpp>
#include <chrono>
#include <thread>

class MemoryItemServer
{
public:
  MemoryItemServer(const std::string &address)
  {
    socket_.setsockopt(ZMQ_ROUTER_MANDATORY, 1);
    socket_.bind(address);
   // monitor_.monitor(socket_, "inproc://monitor-server", ZMQ_EVENT_ALL);
  }

  void put(const std::string &item)
  {
    // ...
  }

  void operator()()
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    zmq::message_t identifier;
    zmq::message_t separator;
    zmq::message_t message;

    {
      socket_.recv(&identifier);
      assert(identifier.more());
      socket_.recv(&separator);
      assert(separator.size() == 0);
      assert(separator.more());
      socket_.recv(&message);

      std::string request = std::string(static_cast<char *>(message.data()), message.size());
      std::cout << "server received " << message.size() << ": " << request << std::endl;
    }

    {
      const std::string item = "Hi!";
      zmq::message_t message(item.size());
      std::copy_n(static_cast<const char *>(item.data()), message.size(),
                  static_cast<char *>(message.data()));

      try
      {
        socket_.send(identifier, ZMQ_SNDMORE);
        socket_.send(separator, ZMQ_SNDMORE);
        socket_.send(message);
        std::cout << "server sent: " << item << std::endl;
      }
      catch (zmq::error_t &error)
      {
        std::cout << "ERROR: " << error.what() << std::endl;
      }
    }
  }

  void stop() {}

private:
  zmq::context_t context_{1};
  zmq::socket_t socket_{context_, ZMQ_ROUTER};
  zmq::monitor_t monitor_;
};
