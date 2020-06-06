#ifndef ZMQ_DEMO_ITEMPRODUCER_HPP
#define ZMQ_DEMO_ITEMPRODUCER_HPP

#include <zmq.hpp>

class ItemProducer {
public:
  explicit ItemProducer(const std::string& distributor_address) {
    distributor_socket_.connect(distributor_address);
  };

  void operator()() {
    while (true) {

      // receive completion messages if available
      while (true) {
        zmq::message_t message;
        const auto result =
            distributor_socket_.recv(message, zmq::recv_flags::dontwait);
        if (!result.has_value()) {
          break;
        }
        size_t id = std::stoull(message.to_string());
        std::cout << "RELEASE " << id << std::endl;
      }

      // wait some time
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // send work item
      std::cout << "GENERATE " << i_ << std::endl;
      distributor_socket_.send(zmq::buffer(std::to_string(i_)));
    }
  }

private:
  zmq::context_t context_{1};
  zmq::socket_t distributor_socket_{context_, ZMQ_REQ};
  size_t i_ = 0;
};

#endif
