#ifndef SHM_IPC_ITEMWORKER_HPP
#define SHM_IPC_ITEMWORKER_HPP

#include "ItemWorkerProtocol.hpp"
#include "log.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

/**
 * Completion sink of an ItemWorker.
 *
 * An item is completed by its destructor, which runs on whichever thread the
 * consumer happens to release it on. The completion is therefore queued here
 * and the communication thread is woken through an inproc socket, so that the
 * COMPLETE message goes out without waiting for the consumer to ask for the
 * next item.
 *
 * The sink is owned through a shared_ptr by both the worker and every item it
 * has issued, so an item outliving its worker stays safe.
 */
class WorkerCompletionSink : public CompletionSink {
public:
  WorkerCompletionSink(std::shared_ptr<zmq::context_t> context,
                       const std::string& wakeup_address)
      : context_(std::move(context)),
        wakeup_socket_(*context_, zmq::socket_type::pair) {
    wakeup_socket_.set(zmq::sockopt::linger, 0);
    // No HWM limit: every wakeup must get through, and they are drained in
    // batches, so the queue stays short.
    wakeup_socket_.connect(wakeup_address);
  }

  void complete(ItemID id) override {
    const std::lock_guard<std::mutex> lock(mutex_);
    completed_items_.push(id);
    wake_up();
  }

  /// Wake the communication thread without reporting a completion.
  void notify() {
    const std::lock_guard<std::mutex> lock(mutex_);
    wake_up();
  }

  /// Take everything completed so far.
  std::queue<ItemID> take() {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::queue<ItemID> result;
    result.swap(completed_items_);
    return result;
  }

private:
  void wake_up() {
    try {
      // A dropped wakeup would delay the completion until the next poll
      // timeout, so the socket keeps its default high-water mark. The queue
      // stays short: it can never hold more wakeups than the worker has items
      // outstanding, and they are drained in batches.
      (void)wakeup_socket_.send(zmq::message_t(), zmq::send_flags::dontwait);
    } catch (const zmq::error_t&) {
      // The communication thread has already gone away
    }
  }

  const std::shared_ptr<zmq::context_t> context_;
  std::mutex mutex_;
  std::queue<ItemID> completed_items_;
  zmq::socket_t wakeup_socket_;
};

/**
 * Client of an ItemDistributor.
 *
 * All communication runs on an internal thread, so that heartbeats and
 * completions are sent independently of how long the consumer takes between
 * calls to get(). The thread is started by the first call to get(); any
 * callbacks must be installed before that.
 */
class ItemWorker {
public:
  using DisconnectCallback = std::function<void(void)>;
  using StateCallback =
      std::function<void(ConnectionState, const std::string& reason)>;

  ItemWorker(std::string distributor_address, WorkerParameters parameters)
      : distributor_address_(std::move(distributor_address)),
        parameters_(std::move(parameters)),
        wakeup_address_("inproc://item-worker-wakeup-" +
                        std::to_string(next_instance_number())),
        wakeup_socket_(*context_, zmq::socket_type::pair) {
    if (parameters_.client_name.empty()) {
      throw std::invalid_argument(
          "WorkerParameters.client_name cannot be empty");
    }
    if (parameters_.stride == 0) {
      throw std::invalid_argument("WorkerParameters.stride cannot be zero");
    }
    if (parameters_.window == 0) {
      throw std::invalid_argument("WorkerParameters.window cannot be zero");
    }
    wakeup_socket_.set(zmq::sockopt::linger, 0);

    wakeup_socket_.bind(wakeup_address_);
    completion_sink_ =
        std::make_shared<WorkerCompletionSink>(context_, wakeup_address_);
    // Register right away, so that no item produced between construction and
    // the first call to get() is missed. Started last, when every member the
    // communication thread touches is initialized.
    thread_ = std::thread([this] { run(); });
  }

  // ItemWorker is non-copyable
  ItemWorker(const ItemWorker&) = delete;
  ItemWorker& operator=(const ItemWorker&) = delete;
  ItemWorker(ItemWorker&&) = delete;
  ItemWorker& operator=(ItemWorker&&) = delete;

  virtual ~ItemWorker() {
    stop();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  /**
   * \brief Install a callback invoked whenever the connection is lost.
   *
   * State changes that happen before the callback is installed are not
   * reported, so install it right after construction.
   */
  void set_disconnect_callback(DisconnectCallback callback) {
    const std::lock_guard<std::mutex> lock(callback_mutex_);
    disconnect_callback_ = std::move(callback);
  }

  /// Install a callback invoked on every connection state change.
  void set_state_callback(StateCallback callback) {
    const std::lock_guard<std::mutex> lock(callback_mutex_);
    state_callback_ = std::move(callback);
  }

  /**
   * Retrieve the next item, blocking until one is available.
   *
   * Returns nullptr once the distributor has announced the end of the stream
   * and all received items have been handed out, or after stop().
   */
  std::shared_ptr<const Item> get() {
    std::unique_lock<std::mutex> lock(mutex_);
    item_available_.wait(lock, [this] {
      return !received_items_.empty() || stopped_ || end_of_stream_;
    });
    if (received_items_.empty()) {
      return nullptr;
    }
    auto item = received_items_.front();
    received_items_.pop();
    return item;
  }

  /// True once the distributor has announced the end of the stream.
  [[nodiscard]] bool eos() const { return end_of_stream_; }

  void stop() {
    stopped_ = true;
    completion_sink_->notify();
    item_available_.notify_all();
  }

  [[nodiscard]] WorkerParameters parameters() const { return parameters_; }

  [[nodiscard]] ConnectionState connection_state() const { return state_; }

  /// Identifier the distributor announced in its WELCOME message.
  [[nodiscard]] std::string distributor_instance_id() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return instance_id_;
  }

  [[nodiscard]] size_t items_received() const { return items_received_; }
  [[nodiscard]] size_t items_completed() const { return items_completed_; }
  [[nodiscard]] size_t reconnect_count() const { return reconnect_count_; }

private:
  static size_t next_instance_number() {
    static std::atomic<size_t> counter{0};
    return counter++;
  }

  void run() {
    while (!stopped_) {
      try {
        const auto now = std::chrono::steady_clock::now();
        if (!distributor_socket_ && now >= reconnect_time_) {
          connect();
        }

        std::array<zmq_pollitem_t, 2> items{};
        items.at(0) = {wakeup_socket_.handle(), 0, ZMQ_POLLIN, 0};
        size_t num_items = 1;
        if (distributor_socket_) {
          items.at(1) = {distributor_socket_->handle(), 0, ZMQ_POLLIN, 0};
          num_items = 2;
        }
        zmq::poll(items.data(), num_items, poll_timeout);

        if ((items.at(0).revents & ZMQ_POLLIN) != 0) {
          drain_wakeups();
        }
        send_pending_completions();
        if (num_items == 2 && (items.at(1).revents & ZMQ_POLLIN) != 0) {
          receive_message();
        }
        check_liveness();
        send_heartbeat_if_due();
      } catch (const WorkerProtocolError& protocol_error) {
        L_(error) << "worker protocol violation: " << protocol_error.what();
        handle_connection_loss(protocol_error.what());
      } catch (const zmq::error_t& zmq_error) {
        if (zmq_error.num() == EINTR) {
          stopped_ = true;
          break;
        }
        L_(error) << "ZMQ: " << zmq_error.what();
        handle_connection_loss(zmq_error.what());
      }
    }
    send_graceful_disconnect();
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      distributor_socket_ = nullptr;
    }
    item_available_.notify_all();
  }

  void connect() {
    distributor_socket_ =
        std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::dealer);
    distributor_socket_->set(zmq::sockopt::linger, 0);
    distributor_socket_->set(zmq::sockopt::heartbeat_ivl,
                             static_cast<int>(heartbeat_interval.count()));
    distributor_socket_->set(zmq::sockopt::heartbeat_timeout,
                             static_cast<int>(peer_timeout.count()));
    distributor_socket_->set(zmq::sockopt::tcp_keepalive, 1);
    // Attach soon after a distributor appears, since items published before
    // the registration are not delivered to this worker
    distributor_socket_->set(zmq::sockopt::reconnect_ivl,
                             static_cast<int>(zmq_reconnect_interval.count()));
    distributor_socket_->connect(distributor_address_);
    last_receive_time_ = std::chrono::steady_clock::now();
    send_register();
    set_state(ConnectionState::Registering, "");
  }

  void send_to_distributor(zmq::multipart_t&& message) {
    if (!distributor_socket_) {
      return;
    }
    // A DEALER socket does not add the empty delimiter frame that the ROUTER
    // on the other side expects from a REQ peer, so add it explicitly.
    message.push(zmq::message_t(0));
    if (!message.send(*distributor_socket_)) {
      throw WorkerProtocolError("message send failed");
    }
    last_send_time_ = std::chrono::steady_clock::now();
  }

  void send_register() {
    zmq::multipart_t message;
    message.addstr(register_verb);
    message.addstr(std::to_string(item_protocol_version));
    message.addstr(std::to_string(parameters_.stride));
    message.addstr(std::to_string(parameters_.offset));
    message.addstr(to_string(parameters_.queue_policy));
    message.addstr(std::to_string(parameters_.group_id));
    message.addstr(std::to_string(parameters_.window));
    message.addstr(parameters_.client_name);
    send_to_distributor(std::move(message));
  }

  void send_heartbeat() {
    zmq::multipart_t message;
    message.addstr(heartbeat_verb);
    send_to_distributor(std::move(message));
  }

  void send_graceful_disconnect() {
    if (!distributor_socket_) {
      return;
    }
    try {
      send_pending_completions();
      zmq::multipart_t message;
      message.addstr(disconnect_verb);
      message.addstr("worker is shutting down");
      send_to_distributor(std::move(message));
    } catch (const std::exception&) {
    }
  }

  void drain_wakeups() {
    zmq::message_t message;
    while (wakeup_socket_.recv(message, zmq::recv_flags::dontwait)) {
    }
  }

  void send_pending_completions() {
    auto completed = completion_sink_->take();
    if (completed.empty()) {
      return;
    }
    if (!distributor_socket_) {
      // The distributor has released these items already
      return;
    }
    zmq::multipart_t message;
    message.addstr(complete_verb);
    while (!completed.empty()) {
      message.addstr(std::to_string(completed.front()));
      completed.pop();
      ++items_completed_;
    }
    send_to_distributor(std::move(message));
  }

  void receive_message() {
    zmq::multipart_t message;
    if (!message.recv(*distributor_socket_)) {
      return;
    }
    last_receive_time_ = std::chrono::steady_clock::now();

    if (message.empty() || !message.at(0).empty()) {
      throw WorkerProtocolError("missing delimiter frame");
    }
    message.pop();
    if (message.empty()) {
      throw WorkerProtocolError("message without a verb");
    }
    const std::string verb = message.popstr();

    if (verb == work_item_verb) {
      handle_work_item(message);
    } else if (verb == welcome_verb) {
      handle_welcome(message);
    } else if (verb == heartbeat_verb) {
      // Nothing to do, the receive time has been updated already
    } else if (verb == end_of_stream_verb) {
      end_of_stream_ = true;
      item_available_.notify_all();
    } else if (verb == disconnect_verb) {
      const std::string reason =
          message.empty() ? "no reason given" : message.popstr();
      handle_connection_loss("disconnected by the distributor: " + reason);
    } else {
      throw WorkerProtocolError("unknown message type: " + verb);
    }
  }

  void handle_welcome(zmq::multipart_t& message) {
    if (message.size() < 2) {
      throw WorkerProtocolError("malformed welcome message");
    }
    const size_t version =
        parse_number_frame(message.popstr(), "protocol version");
    if (version != item_protocol_version) {
      throw WorkerProtocolError("distributor speaks protocol version " +
                                std::to_string(version) +
                                ", this worker speaks version " +
                                std::to_string(item_protocol_version));
    }
    const std::string instance_id = message.popstr();
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      if (!instance_id_.empty() && instance_id_ != instance_id) {
        L_(warning) << "distributor restarted, item ids start over "
                    << "(instance " << instance_id_ << " -> " << instance_id
                    << ")";
      }
      instance_id_ = instance_id;
    }
    reconnect_delay_ = reconnect_interval_min;
    set_state(ConnectionState::Connected, "");
  }

  void handle_work_item(zmq::multipart_t& message) {
    if (message.empty()) {
      throw WorkerProtocolError("work item without an id");
    }
    const ItemID id = parse_number_frame(message.popstr(), "item id");
    std::string payload;
    if (!message.empty()) {
      payload = message.popstr();
    }
    auto item =
        std::make_shared<Item>(completion_sink_, id, std::move(payload));
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      received_items_.push(std::move(item));
    }
    ++items_received_;
    item_available_.notify_one();
  }

  void check_liveness() {
    if (!distributor_socket_) {
      return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (last_receive_time_ + peer_timeout < now) {
      handle_connection_loss("no message from the distributor for " +
                             std::to_string(peer_timeout.count()) + " ms");
    } else if (last_receive_time_ + degraded_timeout < now) {
      if (state_ == ConnectionState::Connected) {
        set_state(ConnectionState::Degraded, "missed heartbeat");
      }
    } else if (state_ == ConnectionState::Degraded) {
      set_state(ConnectionState::Connected, "");
    }
  }

  void send_heartbeat_if_due() {
    if (!distributor_socket_) {
      return;
    }
    if (last_send_time_ + heartbeat_interval <
        std::chrono::steady_clock::now()) {
      send_heartbeat();
    }
  }

  void handle_connection_loss(const std::string& reason) {
    distributor_socket_ = nullptr;
    // Whatever was completed before the connection went away is meaningless
    // now; the distributor released those items when it dropped this worker.
    (void)completion_sink_->take();
    ++reconnect_count_;
    set_state(ConnectionState::Disconnected, reason);
    DisconnectCallback callback;
    {
      const std::lock_guard<std::mutex> lock(callback_mutex_);
      callback = disconnect_callback_;
    }
    if (callback) {
      callback();
    }
    schedule_reconnect();
  }

  void schedule_reconnect() {
    // Randomized exponential backoff, so that many workers do not all return
    // at the same moment after a distributor restart.
    static thread_local std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<int64_t> jitter(reconnect_delay_.count() / 2,
                                                  reconnect_delay_.count());
    reconnect_time_ = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds{jitter(generator)};
    reconnect_delay_ =
        std::min(reconnect_delay_ * 2,
                 std::chrono::milliseconds{reconnect_interval_max});
  }

  void set_state(ConnectionState state, const std::string& reason) {
    if (state_ == state) {
      return;
    }
    state_ = state;
    switch (state) {
    case ConnectionState::Connected:
      L_(info) << "connected to item distributor " << distributor_address_
               << (reconnect_count_ > 0 ? " (reconnected)" : "");
      break;
    case ConnectionState::Degraded:
      L_(warning) << "item distributor " << distributor_address_
                  << " missed a heartbeat";
      break;
    case ConnectionState::Disconnected:
      L_(warning) << "lost item distributor " << distributor_address_ << ": "
                  << reason;
      break;
    case ConnectionState::Registering:
      L_(debug) << "registering with item distributor " << distributor_address_;
      break;
    }
    StateCallback callback;
    {
      const std::lock_guard<std::mutex> lock(callback_mutex_);
      callback = state_callback_;
    }
    if (callback) {
      callback(state, reason);
    }
  }

  const std::string distributor_address_;
  const WorkerParameters parameters_;
  const std::string wakeup_address_;

  // Shared with the completion sink, so that the context outlives any item
  const std::shared_ptr<zmq::context_t> context_ =
      std::make_shared<zmq::context_t>(1);
  zmq::socket_t wakeup_socket_;
  std::shared_ptr<WorkerCompletionSink> completion_sink_;

  mutable std::mutex callback_mutex_;
  DisconnectCallback disconnect_callback_;
  StateCallback state_callback_;

  // Owned by the communication thread
  std::unique_ptr<zmq::socket_t> distributor_socket_;
  std::chrono::steady_clock::time_point last_send_time_ =
      std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last_receive_time_ =
      std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point reconnect_time_ =
      std::chrono::steady_clock::now();
  std::chrono::milliseconds reconnect_delay_ = reconnect_interval_min;

  // Shared between the communication thread and the consumer
  mutable std::mutex mutex_;
  std::condition_variable item_available_;
  std::queue<std::shared_ptr<const Item>> received_items_;
  std::string instance_id_;

  std::thread thread_;
  std::atomic<ConnectionState> state_ = ConnectionState::Disconnected;
  std::atomic<bool> end_of_stream_ = false;
  std::atomic<bool> stopped_ = false;
  std::atomic<size_t> items_received_ = 0;
  std::atomic<size_t> items_completed_ = 0;
  std::atomic<size_t> reconnect_count_ = 0;
};

#endif
