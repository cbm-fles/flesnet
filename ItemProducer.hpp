#ifndef ZMQ_DEMO_ITEMPRODUCER_HPP
#define ZMQ_DEMO_ITEMPRODUCER_HPP

#include <set>

#include <zmq.hpp>

class ItemProducer {
  constexpr static auto wait_time_ = std::chrono::milliseconds{500};

public:
  ItemProducer(std::shared_ptr<zmq::context_t> context,
               const std::string& distributor_address)
      : context_(context),
        distributor_socket_(*context_, zmq::socket_type::pair) {
    distributor_socket_.connect(distributor_address);
  };

  void operator()() {
    while (i_ < 5 || !outstanding_.empty()) {

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
        if (outstanding_.erase(id) != 1) {
          std::cerr << "Error: invalid item " << id << std::endl;
        };
      }

      // wait some time
      std::this_thread::sleep_for(wait_time_);

      if (i_ < 5) {
        // send work item
        outstanding_.insert(i_);
        std::cout << "Producer GENERATE item " << i_ << std::endl;
        distributor_socket_.send(zmq::buffer(std::to_string(i_)));
        ++i_;
      }
    }
    std::cout << "Producer DONE" << std::endl;
  }

private:
  std::shared_ptr<zmq::context_t> context_;
  zmq::socket_t distributor_socket_;
  size_t i_ = 0;
  std::set<size_t> outstanding_;
};

#endif
