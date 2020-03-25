// Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBuilderZeromq.hpp"
#include "MicrosliceDescriptor.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceWorkItem.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include <chrono>
#include <thread>

TimesliceBuilderZeromq::TimesliceBuilderZeromq(
    uint64_t compute_index,
    TimesliceBuffer& timeslice_buffer,
    const std::vector<std::string>& input_server_addresses,
    uint32_t num_compute_nodes,
    uint32_t timeslice_size,
    uint32_t max_timeslice_number,
    volatile sig_atomic_t* signal_status,
    void* zmq_context)
    : compute_index_(compute_index), timeslice_buffer_(timeslice_buffer),
      input_server_addresses_(input_server_addresses),
      num_compute_nodes_(num_compute_nodes), timeslice_size_(timeslice_size),
      max_timeslice_number_(max_timeslice_number),
      signal_status_(signal_status), ts_index_(compute_index_),
      ack_(timeslice_buffer_.get_desc_size_exp()) {
  for (size_t i = 0; i < input_server_addresses_.size(); ++i) {
    auto input_server_address = input_server_addresses_.at(i);

    std::unique_ptr<Connection> c(new Connection{timeslice_buffer_, i});

    c->socket = zmq_socket(zmq_context, ZMQ_REQ);
    assert(c->socket);
    int timeout_ms = 500;
    int rc =
        zmq_setsockopt(c->socket, ZMQ_RCVTIMEO, &timeout_ms, sizeof timeout_ms);
    assert(rc == 0);
    rc =
        zmq_setsockopt(c->socket, ZMQ_SNDTIMEO, &timeout_ms, sizeof timeout_ms);
    assert(rc == 0);

    rc = zmq_connect(c->socket, input_server_address.c_str());
    assert(rc == 0);

    connections_.push_back(std::move(c));
  }
}

TimesliceBuilderZeromq::~TimesliceBuilderZeromq() {
  for (auto& c : connections_) {
    if (c->socket != nullptr) {
      int rc = zmq_close(c->socket);
      assert(rc == 0);
    }
  }
}

void TimesliceBuilderZeromq::operator()() {
  run_begin();
  while (ts_index_ < max_timeslice_number_ && *signal_status_ == 0) {
    run_cycle();
    scheduler_.timer();
  }
  run_end();
}

void TimesliceBuilderZeromq::run_begin() {
  assert(!connections_.empty());
  time_begin_ = std::chrono::high_resolution_clock::now();
  report_status();
}

bool TimesliceBuilderZeromq::run_cycle() {
  auto& c = connections_.at(conn_);

  std::size_t msg_size;

  // send request for timeslice data
  int rc;
  do {
    rc = zmq_send(c->socket, &ts_index_, sizeof(ts_index_), 0);
  } while (rc == -1 && errno == EAGAIN && *signal_status_ == 0);
  if (*signal_status_ != 0) {
    return true;
  }

  // receive desc answer (part 1), do not release
  rc = zmq_msg_init(&c->desc_msg);
  assert(rc == 0);
  do {
    rc = zmq_msg_recv(&c->desc_msg, c->socket, 0);
  } while (rc == -1 && errno == EAGAIN && *signal_status_ == 0);
  if (*signal_status_ != 0) {
    return true;
  }
  assert(rc != -1);
  msg_size = zmq_msg_size(&c->desc_msg);
  if (msg_size == 0) {
    zmq_msg_close(&c->desc_msg);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (msg_size == 0 || *signal_status_ != 0) {
    return true;
  }

  // receive data answer (part 2), do not release
  assert(zmq_msg_more(&c->desc_msg));
  rc = zmq_msg_init(&c->data_msg);
  assert(rc == 0);
  do {
    rc = zmq_msg_recv(&c->data_msg, c->socket, 0);
  } while (rc == -1 && errno == EAGAIN && *signal_status_ == 0);
  if (*signal_status_ != 0) {
    return true;
  }
  assert(rc != -1);

  uint64_t size_required =
      zmq_msg_size(&c->desc_msg) + zmq_msg_size(&c->data_msg);

  while (c->data.size_available_contiguous() < size_required ||
         c->desc.size_available() < 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    handle_timeslice_completions();
  }

  // skip remaining bytes in data buffer to avoid fractured entry
  c->data.skip_buffer_wrap(size_required);

  // generate timeslice component descriptor
  assert(tpos_ == c->desc.write_index());
  c->desc.append(
      {ts_index_, c->data.write_index(), size_required,
       zmq_msg_size(&c->desc_msg) / sizeof(fles::MicrosliceDescriptor)});

  // copy into shared memory and release messages
  c->data.append(static_cast<uint8_t*>(zmq_msg_data(&c->desc_msg)),
                 zmq_msg_size(&c->desc_msg));
  c->data.append(static_cast<uint8_t*>(zmq_msg_data(&c->data_msg)),
                 zmq_msg_size(&c->data_msg));
  zmq_msg_close(&c->desc_msg);
  zmq_msg_close(&c->data_msg);

  ++conn_;
  if (conn_ == connections_.size()) {
    conn_ = 0;

    handle_timeslice_completions();

    timeslice_buffer_.send_work_item(
        {{ts_index_, tpos_, timeslice_size_,
          static_cast<uint32_t>(connections_.size())},
         timeslice_buffer_.get_data_size_exp(),
         timeslice_buffer_.get_desc_size_exp()});
    ++tpos_;
    // next timeslice: round robin
    ts_index_ += num_compute_nodes_;
  }

  return true;
}

void TimesliceBuilderZeromq::run_end() {
  time_end_ = std::chrono::high_resolution_clock::now();

  // wait until all pending timeslices have been acknowledged
  while (acked_ < tpos_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    handle_timeslice_completions();
  }
  assert(timeslice_buffer_.get_num_work_items() == 0);
  assert(timeslice_buffer_.get_num_completions() == 0);
  timeslice_buffer_.send_end_work_item();
  timeslice_buffer_.send_end_completion();
}

void TimesliceBuilderZeromq::handle_timeslice_completions() {
  fles::TimesliceCompletion c;
  while (timeslice_buffer_.try_receive_completion(c)) {
    if (c.ts_pos == acked_) {
      do {
        ++acked_;
      } while (ack_.at(acked_) > c.ts_pos);
      for (auto& conn : connections_) {
        conn->desc.set_read_index(acked_);
        conn->data.set_read_index(conn->desc.at(acked_ - 1).offset +
                                  conn->desc.at(acked_ - 1).size);
      }
    } else {
      ack_.at(c.ts_pos) = c.ts_pos;
    }
  }
}

void TimesliceBuilderZeromq::report_status() {
  constexpr auto interval = std::chrono::seconds(1);

  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  // FIXME: dummy code here...
  auto& c = connections_.at(0);
  BufferStatus status_desc{now, c->desc.size(), acked_, acked_, tpos_};
  BufferStatus status_data{now, c->desc.size(), acked_, acked_, tpos_};

  /*
  double delta_t =
      std::chrono::duration<double, std::chrono::seconds::period>(
          status_desc.time - previous_buffer_status_desc_.time)
          .count();
  double rate_desc = static_cast<double>(status_desc.acked -
                                         previous_buffer_status_desc_.acked) /
                     delta_t;
  double rate_data = static_cast<double>(status_data.acked -
                                         previous_buffer_status_data_.acked) /
                     delta_t;
  */

  L_(debug) << "[c" << compute_index_ << "] desc " << status_desc.percentages()
            << " (used..free) | "
            << human_readable_count(status_desc.acked, true, "")
            << " timeslices";

  L_(debug) << "[c" << compute_index_ << "] data " << status_data.percentages()
            << " (used..free) | "
            << human_readable_count(status_data.acked, true);

  L_(info) << "[c" << compute_index_ << "] |"
           << bar_graph(status_data.vector(), "#._", 20) << "|"
           << bar_graph(status_desc.vector(), "#._", 10) << "| ";

  previous_buffer_status_desc_ = status_desc;
  previous_buffer_status_data_ = status_data;

  scheduler_.add(std::bind(&TimesliceBuilderZeromq::report_status, this),
                 now + interval);
}
