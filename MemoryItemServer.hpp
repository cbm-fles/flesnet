#pragma once

#include <chrono>
#include <iostream>
#include <thread>
#include <zmq.h>
#include <zmq.hpp>

class ConnectionMonitor : public zmq::monitor_t {
  virtual void on_monitor_started() {
    std::cerr << "on_monitor_started" << std::endl;
  }
  virtual void on_event_connected(const zmq_event_t& event_,
                                  const char* addr_) {
    std::cerr << "on_event_connected" << std::endl;
  }
  virtual void on_event_connect_delayed(const zmq_event_t& event_,
                                        const char* addr_) {
    std::cerr << "on_event_connect_delayed" << std::endl;
  }
  virtual void on_event_connect_retried(const zmq_event_t& event_,
                                        const char* addr_) {
    std::cerr << "on_event_connect_retried" << std::endl;
  }
  virtual void on_event_listening(const zmq_event_t& event_,
                                  const char* addr_) {
    std::cerr << "on_event_listening" << std::endl;
  }
  virtual void on_event_bind_failed(const zmq_event_t& event_,
                                    const char* addr_) {
    std::cerr << "on_event_bind_failed" << std::endl;
  }
  virtual void on_event_accepted(const zmq_event_t& event_, const char* addr_) {
    std::cerr << "on_event_accepted" << std::endl;
    std::cerr << event_.value << std::endl;
    std::cerr << addr_ << std::endl;
  }
  virtual void on_event_accept_failed(const zmq_event_t& event_,
                                      const char* addr_) {
    std::cerr << "on_event_accept_failed" << std::endl;
  }
  virtual void on_event_closed(const zmq_event_t& event_, const char* addr_) {
    std::cerr << "on_event_closed" << std::endl;
  }
  virtual void on_event_close_failed(const zmq_event_t& event_,
                                     const char* addr_) {
    std::cerr << "on_event_close_failed" << std::endl;
  }
  virtual void on_event_disconnected(const zmq_event_t& event_,
                                     const char* addr_) {
    std::cerr << "on_event_disconnected" << std::endl;
  }
  virtual void on_event_unknown(const zmq_event_t& event_, const char* addr_) {
    std::cerr << "on_event_unknown" << std::endl;
  }
};

class MemoryItemServer {
public:
  MemoryItemServer(const std::string& address) {
    socket_.setsockopt(ZMQ_ROUTER_MANDATORY, 1);
    socket_.bind(address);

    monitor_thread_ = std::unique_ptr<std::thread>(new std::thread([=]() {
      monitor_.monitor(socket_, "inproc://monitor-server", ZMQ_EVENT_ALL);
    }));
  }

  ~MemoryItemServer() {
    monitor_.abort();
    monitor_thread_->join();
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
      socket_.recv(&identifier);
      assert(identifier.more());
      socket_.recv(&separator);
      assert(separator.size() == 0);
      assert(separator.more());
      socket_.recv(&message);

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
        socket_.send(identifier, ZMQ_SNDMORE);
        socket_.send(separator, ZMQ_SNDMORE);
        socket_.send(message);
        std::cout << "server sent: " << item << std::endl;
      } catch (zmq::error_t& error) {
        std::cout << "ERROR: " << error.what() << std::endl;
      }
    }
  }

  void stop() {}

private:
  zmq::context_t context_{1};
  zmq::socket_t socket_{context_, ZMQ_ROUTER};
  ConnectionMonitor monitor_;
  std::unique_ptr<std::thread> monitor_thread_;
};
