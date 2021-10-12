#ifndef SHM_IPC_ITEMWORKER_HPP
#define SHM_IPC_ITEMWORKER_HPP

#include "ItemWorkerProtocol.hpp"
#include "log.hpp"

#include <cassert>
#include <chrono>
#include <queue>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include <zmq.hpp>

class ItemWorker {
public:
  using DisconnectCallback = std::function<void(void)>;

  ItemWorker(std::string distributor_address, WorkerParameters parameters)
      : distributor_address_(std::move(distributor_address)),
        parameters_(std::move(parameters)) {
    if (parameters_.client_name.empty()) {
      throw std::invalid_argument(
          "WorkerParameters.client_name cannot be empty");
    }
    connect();
  };

  void set_disconnect_callback(DisconnectCallback callback) {
    disconnect_callback_ = callback;
  }

  std::shared_ptr<const Item> get() {
    while (!stopped_) {
      try {
        if (!distributor_socket_) {
          connect();
        } else {
          send_pending_completions();
        }

        zmq::poller_t poller;
        poller.add(*distributor_socket_, zmq::event_flags::pollin);
        std::vector<decltype(poller)::event_type> events(1);
        size_t num_events = poller.wait_all(events, worker_poll_timeout);

        if (num_events > 0) {
          // receive message
          zmq::message_t message;
          (void)distributor_socket_->recv(message);
          auto message_string = message.to_string();
          reset_heartbeat_time();

          if (is_work_item(message_string)) {
            // Handle new work item
            ItemID id{};
            std::string payload;
            std::string command;
            std::stringstream s(message_string);
            s >> command >> id;
            if (s.fail()) {
              throw(WorkerProtocolError("invalid WORK_ITEM message"));
            }
            if (message.more()) {
              (void)distributor_socket_->recv(message);
              payload = message.to_string();
            }
            return std::make_shared<Item>(&completed_items_, id, payload);
          }
          if (message.more()) {
            throw(WorkerProtocolError("unexpected multipart message"));
          }
          if (is_heartbeat(message_string)) {
            send_heartbeat();
          } else if (is_disconnect(message_string)) {
            distributor_socket_ = nullptr;
            disconnect_callback_();
          } else {
            throw(WorkerProtocolError("invalid message type"));
          }
        } else {
          if (heartbeat_is_expired()) {
            throw(WorkerProtocolError("connection heartbeat expired"));
          }
        }
      } catch (WorkerProtocolError& wp_error) {
        L_(error) << "Worker protocol violation: " << wp_error.what();
        distributor_socket_ = nullptr;
        disconnect_callback_();
        std::queue<ItemID>().swap(completed_items_);
      } catch (zmq::error_t& zmq_error) {
        L_(error) << "ZMQ: " << zmq_error.what();
        distributor_socket_ = nullptr;
        disconnect_callback_();
        std::queue<ItemID>().swap(completed_items_);
      }
    }
    return nullptr;
  }

  [[nodiscard]] WorkerParameters parameters() const { return parameters_; }

  void stop() { stopped_ = true; }

private:
  void connect() {
    assert(!distributor_socket_);
    distributor_socket_ =
        std::make_unique<zmq::socket_t>(context_, zmq::socket_type::req);
    distributor_socket_->connect(distributor_address_);
    send_register();
  }

  void send_register() {
    const std::string message_str =
        "REGISTER " + std::to_string(parameters_.stride) + " " +
        std::to_string(parameters_.offset) + " " +
        to_string(parameters_.queue_policy) + " " + parameters_.client_name;
    distributor_socket_->send(zmq::buffer(message_str));
    reset_heartbeat_time();
  }

  void send_heartbeat() {
    const std::string message_str = "HEARTBEAT";
    distributor_socket_->send(zmq::buffer(message_str));
    reset_heartbeat_time();
  }

  void send_completion(ItemID id) {
    const std::string message_str = "COMPLETE " + std::to_string(id);
    distributor_socket_->send(zmq::buffer(message_str));
    reset_heartbeat_time();
  }

  void send_pending_completions() {
    while (!completed_items_.empty()) {
      auto id = completed_items_.front();
      send_completion(id);
      completed_items_.pop();
      items_.erase(id);
    }
  }

  // const std::string receive_message() {}

  static bool is_work_item(const std::string& message) {
    return (message.rfind("WORK_ITEM ", 0) == 0);
  }

  static bool is_heartbeat(const std::string& message) {
    return (message.rfind("HEARTBEAT", 0) == 0);
  }

  static bool is_disconnect(const std::string& message) {
    return (message.rfind("DISCONNECT", 0) == 0);
  }

  [[nodiscard]] bool heartbeat_is_expired() const {
    return (last_heartbeat_time_ + worker_heartbeat_timeout <
            std::chrono::system_clock::now());
  }

  void reset_heartbeat_time() {
    last_heartbeat_time_ = std::chrono::system_clock::now();
  }

  const std::string distributor_address_;
  zmq::context_t context_{1};
  std::unique_ptr<zmq::socket_t> distributor_socket_;
  DisconnectCallback disconnect_callback_;

  const WorkerParameters parameters_{1, 0, WorkerQueuePolicy::QueueAll,
                                     "example_client"};
  std::set<ItemID> items_;
  std::queue<ItemID> completed_items_;
  std::chrono::system_clock::time_point last_heartbeat_time_ =
      std::chrono::system_clock::now();
  bool stopped_ = false;
};

#endif
