#ifndef ZMQ_DEMO_ITEMPRODUCER_HPP
#define ZMQ_DEMO_ITEMPRODUCER_HPP

#include <memory>
#include <set>

#include <zmq.hpp>

using ItemID = size_t;

class ItemProducer {
public:
  ItemProducer(std::shared_ptr<zmq::context_t> context,
               const std::string& distributor_address)
      : context_(std::move(context)),
        distributor_socket_(*context_, zmq::socket_type::pair) {
    distributor_socket_.connect(distributor_address);
  };

  void send_work_item(ItemID id, const std::string& payload) {
    if (payload.empty()) {
      distributor_socket_.send(zmq::buffer(std::to_string(id)));
    } else {
      zmq::message_t message(id);
      zmq::message_t payload_message(payload);
      distributor_socket_.send(message, zmq::send_flags::sndmore);
      distributor_socket_.send(payload_message, zmq::send_flags::none);
    }
  }

  bool try_receive_completion(ItemID* id) {
    zmq::message_t message;
    const auto result =
        distributor_socket_.recv(message, zmq::recv_flags::dontwait);
    if (!result.has_value()) {
      return false;
    }
    *id = std::stoull(message.to_string());
    return true;
  }

private:
  std::shared_ptr<zmq::context_t> context_;
  zmq::socket_t distributor_socket_;
};

#endif
