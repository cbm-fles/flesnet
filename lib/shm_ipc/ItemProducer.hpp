#ifndef SHM_IPC_ITEMPRODUCER_HPP
#define SHM_IPC_ITEMPRODUCER_HPP

#include "ItemID.hpp"
#include "ItemWorkerProtocol.hpp"

#include <string>

#include <zmq.hpp>
#include <zmq_addon.hpp>

class ItemProducer {
public:
  ItemProducer(zmq::context_t& context, const std::string& distributor_address)
      : distributor_socket_(context, zmq::socket_type::pair) {
    distributor_socket_.connect(distributor_address);
  };

  void send_work_item(ItemID id, const std::string& payload) {
    zmq::multipart_t message;
    message.addstr(work_item_verb);
    message.addstr(std::to_string(id));
    if (!payload.empty()) {
      message.addstr(payload);
    }
    message.send(distributor_socket_);
  }

  /// Announce that no further work items will follow.
  void send_end_of_stream() {
    zmq::multipart_t message;
    message.addstr(end_of_stream_verb);
    message.send(distributor_socket_);
  }

  bool try_receive_completion(ItemID* id) {
    zmq::multipart_t message;
    try {
      if (!message.recv(distributor_socket_, ZMQ_DONTWAIT)) {
        return false;
      }
    } catch (zmq::error_t& ex) {
      if (ex.num() == EINTR) {
        return false;
      }
      throw;
    }
    if (message.size() != 2 || message.peekstr(0) != complete_verb) {
      throw WorkerProtocolError("invalid message from item distributor");
    }
    *id = parse_number_frame(message.peekstr(1), "item id");
    return true;
  }

private:
  zmq::socket_t distributor_socket_;
};

#endif
