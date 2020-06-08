#ifndef ZMQ_DEMO_ITEMPRODUCER_HPP
#define ZMQ_DEMO_ITEMPRODUCER_HPP

#include <zmq.hpp>

class ItemProducer {
  constexpr static auto wait_time_ = std::chrono::milliseconds{500};

public:
  explicit ItemProducer(const std::string& distributor_address) {
    distributor_socket_.connect(distributor_address);
  };

  void operator()() {
    while (i_ < 5) {

      // receive completion messages if available
      while (true) {
        zmq::message_t message;
        const auto result =
            distributor_socket_.recv(message, zmq::recv_flags::dontwait);
        if (!result.has_value()) {
          break;
        }
        size_t id = std::stoull(message.to_string());
        std::cout << "Producer RELEASE item " << id << std::endl;
      }

      // wait some time
      std::this_thread::sleep_for(wait_time_);

      // send work item
      std::cout << "Producer GENERATE item " << i_ << std::endl;
      distributor_socket_.send(zmq::buffer(std::to_string(i_)));
      ++i_;
    }
  }

private:
  zmq::context_t context_{1};
  zmq::socket_t distributor_socket_{context_, zmq::socket_type::pair};
  size_t i_ = 0;
};

#endif
