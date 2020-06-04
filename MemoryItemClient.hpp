#pragma once

#include <iostream>
#include <zmq.hpp>

class MemoryItemClient
{
public:
  MemoryItemClient(const std::string &address)
  {
    socket_.connect(address);
  };

  std::string get()
  {
    if (eos_flag)
    {
      return nullptr;
    }

    // ...
  }

  void run()
  {
    {
      const std::string item = "Hello!";
      zmq::message_t message(item.size());
      std::copy_n(static_cast<const char *>(item.data()), message.size(),
                  static_cast<char *>(message.data()));
      socket_.send(message);
      std::cout << "client sent: " << item << std::endl;
    }
    {
      zmq::message_t message;
      socket_.recv(&message);
      std::string reply = std::string(static_cast<char *>(message.data()), message.size());
      std::cout << "client received: " << reply << std::endl;
    }
  }

  bool eos() const { return eos_flag; }

private:
  zmq::context_t context_{1};
  zmq::socket_t socket_{context_, ZMQ_REQ};

  bool eos_flag = false;
};
