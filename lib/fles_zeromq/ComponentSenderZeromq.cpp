// Copyright 2012-2013, 2016 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2024 Florian Schintke <schintke@zib.de>

#include "ComponentSenderZeromq.hpp"
#include "DualRingBuffer.hpp"        // InputBufferReadInterface
#include "MicrosliceDescriptor.hpp"
#include "Monitor.hpp"
#include "RingBufferView.hpp"
#include "System.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <zmq.h>

ComponentSenderZeromq::ComponentSenderZeromq(
    uint64_t input_index,
    InputBufferReadInterface& data_source,
    const std::string& listen_address,
    uint32_t timeslice_size,
    uint32_t overlap_size,
    uint32_t max_timeslice_number,
    volatile sig_atomic_t* signal_status,
    void* zmq_context,
    cbm::Monitor* monitor)
    : input_index_(input_index), data_source_(data_source),
      timeslice_size_(timeslice_size), overlap_size_(overlap_size),
      max_timeslice_number_(max_timeslice_number),
      signal_status_(signal_status),
      min_acked_({data_source.desc_buffer().size() / 4,
                  data_source.data_buffer().size() / 4}),
      monitor_(monitor) {
  start_index_ = sent_ = acked_ = cached_acked_ = data_source.get_read_index();

  size_t min_ack_buffer_size =
      (data_source_.desc_buffer().size() / timeslice_size_ + 1) * 2;
  ack_.alloc_with_size(min_ack_buffer_size);

  socket_ = zmq_socket(zmq_context, ZMQ_REP);
  assert(socket_);
  int timeout_ms = 500;
  [[maybe_unused]] int rc =
      zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms, sizeof timeout_ms);
  assert(rc == 0);
  rc = zmq_setsockopt(socket_, ZMQ_SNDTIMEO, &timeout_ms, sizeof timeout_ms);
  assert(rc == 0);

  rc = zmq_bind(socket_, listen_address.c_str());
  assert(rc == 0);

  hostname_ = fles::system::current_hostname();
}

ComponentSenderZeromq::~ComponentSenderZeromq() {
  if (socket_ != nullptr) {
    [[maybe_unused]] int rc = zmq_close(socket_);
    assert(rc == 0);
  }
}

void ComponentSenderZeromq::operator()() {
  run_begin();
  while (acked_ts2_ / 2 < max_timeslice_number_ && *signal_status_ == 0) {
    run_cycle();
    scheduler_.timer();
  }
  run_end();
}

void ComponentSenderZeromq::run_begin() {
  data_source_.proceed();
  time_begin_ = std::chrono::high_resolution_clock::now();
  report_status();
}

bool ComponentSenderZeromq::run_cycle() {
  process_pending_acks();

  zmq_msg_t request;
  [[maybe_unused]] int rc = zmq_msg_init(&request);
  assert(rc == 0);

  int len = zmq_msg_recv(&request, socket_, 0);
  if (len == -1 && errno == EAGAIN) {
    // timeout reached
    return true;
  }
  assert(len != -1);

  assert(len == sizeof(uint64_t));
  uint64_t timeslice = *static_cast<uint64_t*>(zmq_msg_data(&request));
  zmq_msg_close(&request);

  try_send_timeslice(timeslice);
  data_source_.proceed();

  return true;
}

void ComponentSenderZeromq::run_end() {
  sync_data_source();
  time_end_ = std::chrono::high_resolution_clock::now();
}

void enqueue_ack(void* /* data */, void* hint) {
  assert(hint);
  auto* ack = static_cast<ComponentSenderZeromq::Acknowledgment*>(hint);
  // enqueue achnowledgment for later processing by the classes own thread
  std::lock_guard<std::mutex> lock(ack->server->pending_acks_mutex_);
  ack->server->pending_acks_.emplace(ack);
}

void ComponentSenderZeromq::process_pending_acks() {
  std::lock_guard<std::mutex> lock(pending_acks_mutex_);
  while (!pending_acks_.empty()) {
    auto* ack = pending_acks_.front();
    ack_timeslice(ack->timeslice, ack->is_data);
    delete ack; // NOLINT
    pending_acks_.pop();
  }
}

bool ComponentSenderZeromq::try_send_timeslice(uint64_t ts) {
  assert(ts >= acked_ts2_ / 2);

  uint64_t desc_offset = ts * timeslice_size_ + start_index_.desc;
  uint64_t desc_length = timeslice_size_ + overlap_size_;

  // check if complete timeslice is available in the input buffer
  if (write_index_desc_ < desc_offset + desc_length) {
    data_source_.proceed();
    write_index_desc_ = data_source_.get_write_index().desc;
    if (write_index_desc_ < desc_offset + desc_length) {
      // send empty message
      zmq_msg_t msg;
      zmq_msg_init_size(&msg, 0);
      int rc;
      do {
        rc = zmq_msg_send(&msg, socket_, 0);
      } while (rc == -1 && errno == EAGAIN && *signal_status_ == 0);
      return false;
    }
  }

  // part 1: descriptors
  if (desc_offset + desc_length > sent_.desc) {
    sent_.desc = desc_offset + desc_length;
  }
  auto desc_msg = create_message(data_source_.desc_buffer(), desc_offset,
                                 desc_length, ts, false);
  int rc;
  do {
    rc = zmq_msg_send(&desc_msg, socket_, ZMQ_SNDMORE);
  } while (rc == -1 && errno == EAGAIN && *signal_status_ == 0);

  // part 2: data
  uint64_t data_offset = data_source_.desc_buffer().at(desc_offset).offset;
  uint64_t data_end =
      data_source_.desc_buffer().at(desc_offset + desc_length - 1).offset +
      data_source_.desc_buffer().at(desc_offset + desc_length - 1).size;
  assert(data_end >= data_offset);
  uint64_t data_length = data_end - data_offset;

  if (data_offset + data_length > sent_.data) {
    sent_.data = data_offset + data_length;
  }
  auto data_msg = create_message(data_source_.data_buffer(), data_offset,
                                 data_length, ts, true);
  do {
    rc = zmq_msg_send(&data_msg, socket_, 0);
  } while (rc == -1 && errno == EAGAIN && *signal_status_ == 0);

  return true;
}

template <typename T_>
zmq_msg_t ComponentSenderZeromq::create_message(RingBufferView<T_>& buf,
                                                uint64_t offset,
                                                uint64_t length,
                                                uint64_t ts,
                                                bool is_data) {
  zmq_msg_t msg;

  if (length == 0) {
    // zero chunks
    zmq_msg_init_size(&msg, 0);
    ack_timeslice(ts, is_data);
  } else if ((offset & buf.size_mask()) <=
             ((offset + length - 1) & buf.size_mask())) {
    // one chunk
    auto* data = &buf.at(offset);
    size_t bytes = sizeof(T_) * length;
    auto* hint = new Acknowledgment{this, ts, is_data}; // NOLINT
    zmq_msg_init_data(&msg, data, bytes, enqueue_ack, hint);
  } else {
    // two chunks
    auto* data1 = &buf.at(offset);
    size_t size1 = buf.size() - (offset & buf.size_mask());
    auto* data2 = buf.ptr();
    size_t size2 = length - buf.size() + (offset & buf.size_mask());
    zmq_msg_init_size(&msg, (size1 + size2) * sizeof(T_));
    auto msg_buf = static_cast<T_*>(zmq_msg_data(&msg));
    std::copy_n(data1, size1, msg_buf);
    std::copy_n(data2, size2, msg_buf + size1);
    ack_timeslice(ts, is_data);
  }

  return msg;
}

void ComponentSenderZeromq::ack_timeslice(uint64_t ts, bool is_data) {
  // use ts2 and acked_ts2_ to handle desc and data sequentially
  uint64_t ts2 = ts * 2 + (is_data ? 1 : 0);
  assert(ts2 >= acked_ts2_);
  if (ts2 != acked_ts2_) {
    // transmission has been reordered, store completion information
    ack_.at(ts2) = ts2;
  } else {
    // completion is for earliest pending timeslice, update indices
    do {
      ++acked_ts2_;
    } while (ack_.at(acked_ts2_) > ts2);
    acked_.desc = acked_ts2_ / 2 * timeslice_size_ + start_index_.desc;
    acked_.data = data_source_.desc_buffer().at(acked_.desc - 1).offset +
                  data_source_.desc_buffer().at(acked_.desc - 1).size;
    if (acked_.data >= cached_acked_.data + min_acked_.data ||
        acked_.desc >= cached_acked_.desc + min_acked_.desc) {
      cached_acked_ = acked_;
      data_source_.set_read_index(cached_acked_);
    }
  }
}

void ComponentSenderZeromq::sync_data_source() {
  if (acked_.data > cached_acked_.data || acked_.desc > cached_acked_.desc) {
    cached_acked_ = acked_;
    data_source_.set_read_index(cached_acked_);
  }
}

void ComponentSenderZeromq::report_status() {
  constexpr auto interval = std::chrono::seconds(1);

  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  DualIndex written = data_source_.get_write_index();

  // if data_source.written pointers are lagging behind due to lazy updates,
  // use sent value instead
  // PAL: Not sure if needed for ZMQ, but added to ensure same behavior as
  //      rdma implementation
  uint64_t written_desc = written.desc;
  if (written_desc < sent_.desc) {
    written_desc = sent_.desc;
  }
  uint64_t written_data = written.data;
  if (written_data < sent_.data) {
    written_data = sent_.data;
  }

  SendBufferStatus status_desc{now,
                               data_source_.desc_buffer().size(),
                               cached_acked_.desc,
                               acked_.desc,
                               sent_.desc,
                               written_desc};
  SendBufferStatus status_data{now,
                               data_source_.data_buffer().size(),
                               cached_acked_.data,
                               acked_.data,
                               sent_.data,
                               written_data};

  double delta_t =
      std::chrono::duration<double, std::chrono::seconds::period>(
          status_desc.time - previous_send_buffer_status_desc_.time)
          .count();
  double rate_desc =
      static_cast<double>(status_desc.acked -
                          previous_send_buffer_status_desc_.acked) /
      delta_t;
  double rate_data =
      static_cast<double>(status_data.acked -
                          previous_send_buffer_status_data_.acked) /
      delta_t;

  // retrieve SubsystemIdentifier and EquipmentIdentifier
  // from most current MicrosliceDescriptor
  auto sys_id = static_cast<fles::Subsystem>(0);
  std::string eq_id("Undefined");
  if (written_desc > 0) {
    sys_id = static_cast<fles::Subsystem>(
        data_source_.desc_buffer().at(written_desc - 1).sys_id);
    std::stringstream eq_id_ss;
    eq_id_ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
             << data_source_.desc_buffer().at(written_desc - 1).eq_id;
    eq_id = eq_id_ss.str();
  }

  L_(debug) << "[i" << input_index_ << "] desc " << status_desc.percentages()
            << " (used..free) | "
            << human_readable_count(status_desc.acked, true, "") << " ("
            << human_readable_count(rate_desc, true, "Hz") << ")";

  L_(debug) << "[i" << input_index_ << "] data " << status_data.percentages()
            << " (used..free) | "
            << human_readable_count(status_data.acked, true) << " ("
            << human_readable_count(rate_data, true, "B/s") << ")";

  L_(info) << "[i" << input_index_ << "] |"
           << bar_graph(status_data.vector(), "#x._", 20) << "|"
           << bar_graph(status_desc.vector(), "#x._", 10) << "| "
           << human_readable_count(rate_data, true, "B/s") << " ("
           << human_readable_count(rate_desc, true, "Hz") << ")";

  if (monitor_ != nullptr) {
    monitor_->QueueMetric("send_buffer_status",
                          {{"host", hostname_},
                           {"input_index", std::to_string(input_index_)},
                           {"sys_id", fles::to_string(sys_id)},
                           {"eq_id", eq_id}},
                          {{"data_used", status_data.used()},
                           {"data_sending", status_data.sending()},
                           {"data_freeing", status_data.freeing()},
                           {"data_free", status_data.unused()},
                           {"data_rate", rate_data},
                           {"desc_used", status_desc.used()},
                           {"desc_sending", status_desc.sending()},
                           {"desc_freeing", status_desc.freeing()},
                           {"desc_free", status_desc.unused()},
                           {"desc_rate", rate_desc}});
  }

  previous_send_buffer_status_desc_ = status_desc;
  previous_send_buffer_status_data_ = status_data;

  scheduler_.add([this] { report_status(); }, now + interval);
}
