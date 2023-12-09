#ifndef SHM_IPC_ITEMPRODUCER_HPP
#define SHM_IPC_ITEMPRODUCER_HPP

#include <cstddef>
#include <zmq.hpp>

using ItemID = size_t;

class ItemProducer {
public:
  ItemProducer(zmq::context_t& context, const std::string& distributor_address)
      : distributor_socket_(context, zmq::socket_type::pair) {
    distributor_socket_.connect(distributor_address);
  };

  void send_work_item(ItemID id, const std::string& payload) {
    if (payload.empty()) {
      distributor_socket_.send(zmq::buffer(std::to_string(id)));
    } else {
      zmq::message_t message(std::to_string(id));
      zmq::message_t payload_message(payload);
      distributor_socket_.send(message, zmq::send_flags::sndmore);
      distributor_socket_.send(payload_message, zmq::send_flags::none);
    }
  }

  bool try_receive_completion(ItemID* id) {
    zmq::message_t message;
    try {
      const auto result =
          distributor_socket_.recv(message, zmq::recv_flags::dontwait);
      if (!result.has_value()) {
        return false;
      }
    } catch (zmq::error_t& ex) {
      if (ex.num() == EINTR) {
        return false;
      }
      throw;
    }
    *id = std::stoull(message.to_string());
    return true;
  }

private:
  zmq::socket_t distributor_socket_;
};

#endif
