// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "InputChannelSender.hpp"

namespace tl_libfabric {
InputChannelSender::InputChannelSender(
    uint64_t input_index,
    InputBufferReadInterface& data_source,
    const std::vector<std::string> compute_hostnames,
    const std::vector<std::string> compute_services,
    uint32_t timeslice_size,
    uint32_t overlap_size,
    uint32_t max_timeslice_number,
    std::string input_node_name,
    uint32_t scheduler_interval_length,
    std::string log_directory,
    bool enable_logging)
    : ConnectionGroup(input_node_name), input_index_(input_index),
      data_source_(data_source), compute_hostnames_(compute_hostnames),
      compute_services_(compute_services), timeslice_size_(timeslice_size),
      overlap_size_(overlap_size), max_timeslice_number_(max_timeslice_number),
      min_acked_desc_(data_source.desc_buffer().size() / 4),
      min_acked_data_(data_source.data_buffer().size() / 4) {

  start_index_desc_ = sent_desc_ = acked_desc_ = cached_acked_desc_ =
      data_source.get_read_index().desc;
  start_index_data_ = sent_data_ = acked_data_ = cached_acked_data_ =
      data_source.get_read_index().data;

  size_t min_ack_buffer_size =
      data_source_.desc_buffer().size() / timeslice_size_ + 1;
  ack_.alloc_with_size(min_ack_buffer_size);

  if (Provider::getInst()->is_connection_oriented()) {
    connection_oriented_ = true;
  } else {
    connection_oriented_ = false;
  }

  InputSchedulerOrchestrator::initialize(
      input_index, compute_hostnames.size(),
      ConstVariables::INIT_HEARTBEAT_TIMEOUT,
      ConstVariables::HEARTBEAT_TIMEOUT_HISTORY_SIZE,
      ConstVariables::HEARTBEAT_TIMEOUT_FACTOR,
      ConstVariables::HEARTBEAT_INACTIVE_FACTOR,
      ConstVariables::HEARTBEAT_INACTIVE_RETRY_COUNT, scheduler_interval_length,
      data_source.get_write_index().desc, (timeslice_size + overlap_size),
      start_index_desc_, timeslice_size, log_directory, enable_logging);
  InputSchedulerOrchestrator::update_data_source_desc(
      data_source.get_write_index().desc);
}

InputChannelSender::~InputChannelSender() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  if (mr_desc_ != nullptr) {
    fi_close((struct fid*)mr_desc_);
    mr_desc_ = nullptr;
  }

  if (mr_data_ != nullptr) {
    fi_close((struct fid*)(mr_data_));
    mr_data_ = nullptr;
  }
#pragma GCC diagnostic pop
}

void InputChannelSender::report_status() {
  constexpr auto interval = std::chrono::seconds(1);

  // if data_source.written pointers are lagging behind due to lazy updates,
  // use sent value instead
  uint64_t written_desc = data_source_.get_write_index().desc;
  if (written_desc < sent_desc_) {
    written_desc = sent_desc_;
  }
  uint64_t written_data = data_source_.get_write_index().data;
  if (written_data < sent_data_) {
    written_data = sent_data_;
  }

  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  SendBufferStatus status_desc{now,
                               data_source_.desc_buffer().size(),
                               cached_acked_desc_,
                               acked_desc_,
                               sent_desc_,
                               written_desc};
  SendBufferStatus status_data{now,
                               data_source_.data_buffer().size(),
                               cached_acked_data_,
                               acked_data_,
                               sent_data_,
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

  L_(debug) << "[i" << input_index_ << "] desc " << status_desc.percentages()
            << " (used..free) | "
            << human_readable_count(status_desc.acked, true, "") << " ("
            << human_readable_count(rate_desc, true, "Hz") << ")";

  L_(debug) << "[i" << input_index_ << "] data " << status_data.percentages()
            << " (used..free) | "
            << human_readable_count(status_data.acked, true) << " ("
            << human_readable_count(rate_data, true, "B/s") << ")";

  L_(info) << "[i" << input_index_ << "]   |"
           << bar_graph(status_data.vector(), "#x._", 20) << "|"
           << bar_graph(status_desc.vector(), "#x._", 10) << "| "
           << human_readable_count(rate_data, true, "B/s") << " ("
           << human_readable_count(rate_desc, true, "Hz") << ")";

  previous_send_buffer_status_desc_ = status_desc;
  previous_send_buffer_status_data_ = status_data;

  scheduler_.add(std::bind(&InputChannelSender::report_status, this),
                 now + interval);
}

void InputChannelSender::sync_data_source(bool schedule) {
  if (acked_data_ > cached_acked_data_ || acked_desc_ > cached_acked_desc_) {
    cached_acked_data_ = acked_data_;
    cached_acked_desc_ = acked_desc_;
    data_source_.set_read_index({cached_acked_desc_, cached_acked_data_});
  }

  if (schedule) {
    auto now = std::chrono::system_clock::now();
    scheduler_.add(std::bind(&InputChannelSender::sync_data_source, this, true),
                   now + std::chrono::milliseconds(100));
  }
}

void InputChannelSender::sync_heartbeat() {
  InputSchedulerOrchestrator::update_data_source_desc(
      data_source_.get_write_index().desc);
  HeartbeatFailedNodeInfo* failed_connection =
      InputSchedulerOrchestrator::get_timed_out_connection();
  if (failed_connection->index ==
      ConstVariables::MINUS_ONE) { // Check inactive connections
    send_heartbeat_to_inactive_connections();
  } else { // Send timeout message to all active connections
    for (auto& conn : conn_) {
      if (conn->request_finalize_flag() && !conn->done()) {
        mark_connection_completed(conn->index());
      } else {
        if (!InputSchedulerOrchestrator::is_connection_timed_out(
                conn->index()) &&
            !conn->done()) {
          conn->prepare_heartbeat(failed_connection);
        }
      }
    }
  }
  // TODO 1 second?
  scheduler_.add(std::bind(&InputChannelSender::sync_heartbeat, this),
                 std::chrono::system_clock::now() + std::chrono::seconds(1));
}

void InputChannelSender::send_timeslices() {

  uint64_t up_to_timeslice =
      InputSchedulerOrchestrator::get_last_timeslice_to_send();

  uint32_t conn_index = input_index_ % conn_.size();
  do {
    uint64_t next_ts =
        InputSchedulerOrchestrator::get_connection_next_timeslice(conn_index);

    if (next_ts != ConstVariables::MINUS_ONE && next_ts <= up_to_timeslice &&
        next_ts <= max_timeslice_number_ &&
        try_send_timeslice(next_ts, conn_index)) {
      conn_[conn_index]->set_last_sent_timeslice(next_ts);
    }
    conn_index = (conn_index + 1) % conn_.size();
  } while (conn_index != (input_index_ % conn_.size()));

  if (InputSchedulerOrchestrator::get_sent_timeslices() <=
      max_timeslice_number_)
    scheduler_.add(std::bind(&InputChannelSender::send_timeslices, this),
                   std::chrono::system_clock::now() +
                       std::chrono::microseconds(
                           InputSchedulerOrchestrator::get_next_fire_time()));
}

void InputChannelSender::bootstrap_with_connections() {
  connect();
  while (connected_ != compute_hostnames_.size()) {
    poll_cm_events();
  }
}

void InputChannelSender::bootstrap_wo_connections() {
  // domain, cq, av
  init_context(Provider::getInst()->get_info(), compute_hostnames_,
               compute_services_);
  LibfabricBarrier::create_barrier_instance(input_index_, pd_, false);

  conn_.resize(compute_hostnames_.size());
  // setup connections objects
  for (unsigned int i = 0; i < compute_hostnames_.size(); ++i) {
    std::unique_ptr<InputChannelConnection> connection =
        create_input_node_connection(i);
    // creates endpoint
    connection->connect(compute_hostnames_[i], compute_services_[i], pd_,
                        completion_queue(i), av_, fi_addrs[i]);
    conn_.at(i) = (std::move(connection));
  }
  int i = 0;
  while (connected_ != compute_hostnames_.size()) {
    poll_completion();
    i++;
    if (i == 1000000) { // TODO retry for connectionless
      i = 0;
      // reconnecting
      for (unsigned int i = 0; i < compute_hostnames_.size(); ++i) {
        if (connected_indexes_.find(i) == connected_indexes_.end()) {
          L_(info) << "retrying to connect to " << compute_hostnames_[i] << ":"
                   << compute_services_[i];
          conn_.at(i)->reconnect();
        }
      }
    }
  }
}

/// The thread main function.
void InputChannelSender::operator()() {
  try {

    if (Provider::getInst()->is_connection_oriented()) {
      bootstrap_with_connections();
    } else {
      bootstrap_wo_connections();
    }

    data_source_.proceed();

    LibfabricBarrier::get_instance()->call_barrier();

    time_begin_ = std::chrono::high_resolution_clock::now();
    InputSchedulerOrchestrator::update_input_begin_time(time_begin_);

    for (uint32_t indx = 0; indx < conn_.size(); indx++) {
      conn_[indx]->set_time_MPI(time_begin_);
    }

    sync_buffer_positions();
    sync_data_source(true);
    sync_heartbeat();
    report_status();
    send_timeslices();

    while (InputSchedulerOrchestrator::get_sent_timeslices() <=
               max_timeslice_number_ &&
           !abort_) {
      scheduler_.timer();
      poll_completion();
      update_compute_schedulers();
      data_source_.proceed();
    }

    L_(info) << "[i" << input_index_ << "]"
             << "All timeslices are sent.  wait for pending send completions!";

    // wait for pending send completions
    while (acked_desc_ <
           timeslice_size_ * InputSchedulerOrchestrator::get_sent_timeslices() +
               start_index_desc_) {
      poll_completion();
      scheduler_.timer();
    }
    sync_data_source(false);

    L_(debug) << "[i " << input_index_ << "] "
              << "Finalize Connections";
    for (auto& c : conn_) {
      c->finalize(abort_);
    }

    L_(debug) << "[i" << input_index_ << "] "
              << "SENDER loop done";
    while (!all_done_) {
      poll_completion();
      scheduler_.timer();
    }
    time_end_ = std::chrono::high_resolution_clock::now();

    if (connection_oriented_) {
      disconnect();
    }

    while (connected_ != 0) {
      poll_cm_events();
    }

    InputSchedulerOrchestrator::generate_log_files();
    summary();
  } catch (std::exception& e) {
    L_(fatal) << "exception in InputChannelSender: " << e.what();
  }
}

bool InputChannelSender::try_send_timeslice(uint64_t timeslice, uint32_t cn) {
  // wait until a complete timeslice is available in the input buffer
  uint64_t desc_offset = timeslice * timeslice_size_ + start_index_desc_;
  uint64_t desc_length = timeslice_size_ + overlap_size_;

  if (write_index_desc_ < desc_offset + desc_length) {
    write_index_desc_ = data_source_.get_write_index().desc;
  }
  // check if microslice no. (desc_offset + desc_length - 1) is avail
  if (write_index_desc_ >= desc_offset + desc_length) {
    InputSchedulerOrchestrator::log_timeslice_IB_blocked(cn, timeslice, true);
    uint64_t data_offset = data_source_.desc_buffer().at(desc_offset).offset;
    uint64_t data_end =
        data_source_.desc_buffer().at(desc_offset + desc_length - 1).offset +
        data_source_.desc_buffer().at(desc_offset + desc_length - 1).size;
    assert(data_end >= data_offset);

    uint64_t data_length = data_end - data_offset;
    uint64_t total_length =
        data_length + desc_length * sizeof(fles::MicrosliceDescriptor);

    if (false) {
      L_(debug) << "SENDER working on timeslice " << timeslice << " to " << cn
                << ", microslices " << desc_offset << ".."
                << (desc_offset + desc_length - 1) << ", data bytes "
                << data_offset << ".." << (data_offset + data_length - 1);
    }

    if (!conn_[cn]->write_request_available()) {
      L_(debug) << "[" << input_index_ << "]"
                << "max # of writes to " << cn;
      InputSchedulerOrchestrator::log_timeslice_MR_blocked(cn, timeslice);
      return false;
    }
    InputSchedulerOrchestrator::log_timeslice_MR_blocked(cn, timeslice, true);

    // number of bytes to skip in advance (to avoid buffer wrap)
    uint64_t skip = conn_[cn]->skip_required(total_length);
    total_length += skip;

    if (conn_[cn]->check_for_buffer_space(total_length, 1)) {
      if (post_send_data(timeslice, cn, desc_offset, desc_length, data_offset,
                         data_length, skip)) {
        InputSchedulerOrchestrator::log_timeslice_CB_blocked(cn, timeslice,
                                                             true);

        // conn_[cn]->inc_write_pointers(total_length, 1);
        conn_[cn]->add_timeslice_data_address(total_length, 1);
        InputSchedulerOrchestrator::mark_timeslice_transmitted(cn, timeslice,
                                                               total_length);
        if (data_end > sent_data_) { // This if condition is needed when the
                                     // timeslice transmissions are out of order
          sent_desc_ = desc_offset + desc_length;
          sent_data_ = data_end;
        }
        return true;
      }
    } else {
      InputSchedulerOrchestrator::log_timeslice_CB_blocked(cn, timeslice);
    }
  } else {
    InputSchedulerOrchestrator::log_timeslice_IB_blocked(cn, timeslice);
  }

  return false;
}

std::unique_ptr<InputChannelConnection>
InputChannelSender::create_input_node_connection(uint_fast16_t index) {
  // TODO: What is the best value?
  unsigned int max_send_wr = 8000; // ???  IB hca
  // unsigned int max_send_wr = 495; // ??? libfabric for verbs
  // unsigned int max_send_wr = 256; // ??? libfabric for sockets

  // limit pending write requests so that send queue and completion queue
  // do not overflow
  unsigned int max_pending_write_requests = std::min(
      static_cast<unsigned int>((max_send_wr - 1) / 3),
      static_cast<unsigned int>((num_cqe_ - 1) / compute_hostnames_.size()));

  std::unique_ptr<InputChannelConnection> connection(new InputChannelConnection(
      eq_, index, input_index_, max_send_wr, max_pending_write_requests));
  return connection;
}

void InputChannelSender::connect() {
  if (pd_ == nullptr) { // pd, cq2, av
    init_context(Provider::getInst()->get_info(), compute_hostnames_,
                 compute_services_);
    LibfabricBarrier::create_barrier_instance(input_index_, pd_, false);
  }

  conn_.resize(compute_hostnames_.size());
  uint32_t count = 0;
  unsigned int i = input_index_ % compute_hostnames_.size();
  while (count < compute_hostnames_.size()) {
    std::unique_ptr<InputChannelConnection> connection =
        create_input_node_connection(i);
    connection->connect(compute_hostnames_[i], compute_services_[i], pd_,
                        completion_queue(i), av_, FI_ADDR_UNSPEC);
    // conn_.push_back(std::move(connection));
    conn_.at(i) = (std::move(connection));
    ++count;
    i = (i + 1) % compute_hostnames_.size();
  }
}

void InputChannelSender::on_connected(struct fid_domain* pd) {
  if (mr_data_ == nullptr) {
    // Register memory regions.
    int err =
        fi_mr_reg(pd, const_cast<uint8_t*>(data_source_.data_buffer().ptr()),
                  data_source_.data_buffer().bytes(), FI_WRITE, 0,
                  Provider::requested_key++, 0, &mr_data_, nullptr);
    if (err != 0) {
      L_(fatal) << "fi_mr_reg failed for data_send_buffer: " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_mr_reg failed for data_send_buffer");
    }

    if (mr_data_ == nullptr) {
      L_(fatal) << "fi_mr_reg failed for mr_data: " << strerror(errno);
      throw LibfabricException("registration of memory region failed");
    }

    err = fi_mr_reg(pd,
                    const_cast<fles::MicrosliceDescriptor*>(
                        data_source_.desc_buffer().ptr()),
                    data_source_.desc_buffer().bytes(), FI_WRITE, 0,
                    Provider::requested_key++, 0, &mr_desc_, nullptr);
    if (err != 0) {
      L_(fatal) << "fi_mr_reg failed for desc_send_buffer: " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_mr_reg failed for desc_send_buffer");
    }

    if (mr_desc_ == nullptr) {
      L_(fatal) << "fi_mr_reg failed for mr_desc: " << strerror(errno);
      throw LibfabricException("registration of memory region failed");
    }
  }
}

void InputChannelSender::on_rejected(struct fi_eq_err_entry* event) {
  L_(debug) << "InputChannelSender:on_rejected";

  InputChannelConnection* conn =
      static_cast<InputChannelConnection*>(event->fid->context);

  conn->on_rejected(event);
  uint_fast16_t i = conn->index();
  conn_.at(i) = nullptr;

  L_(debug) << "retrying: " << i;
  // immediately initiate retry
  std::unique_ptr<InputChannelConnection> connection =
      create_input_node_connection(i);
  connection->connect(compute_hostnames_[i], compute_services_[i], pd_,
                      completion_queue(i), av_, FI_ADDR_UNSPEC);
  conn_.at(i) = std::move(connection);
}

std::string InputChannelSender::get_state_string() {
  std::ostringstream s;

  s << "/--- desc buf ---" << std::endl;
  s << "|";
  for (unsigned int i = 0; i < data_source_.desc_buffer().size(); ++i)
    s << " (" << i << ")" << data_source_.desc_buffer().at(i).offset;
  s << std::endl;
  s << "| acked_desc_ = " << acked_desc_ << std::endl;
  s << "/--- data buf ---" << std::endl;
  s << "|";
  for (unsigned int i = 0; i < data_source_.data_buffer().size(); ++i)
    s << " (" << i << ")" << std::hex << data_source_.data_buffer().at(i)
      << std::dec;
  s << std::endl;
  s << "| acked_data_ = " << acked_data_ << std::endl;
  s << "\\---------";

  return s.str();
}

bool InputChannelSender::post_send_data(uint64_t timeslice,
                                        int cn,
                                        uint64_t desc_offset,
                                        uint64_t desc_length,
                                        uint64_t data_offset,
                                        uint64_t data_length,
                                        uint64_t skip) {
  int num_sge = 0;
  struct iovec sge[4];
  void* descs[4];
  // descriptors
  if ((desc_offset & data_source_.desc_buffer().size_mask()) <=
      ((desc_offset + desc_length - 1) &
       data_source_.desc_buffer().size_mask())) {
    // one chunk
    sge[num_sge].iov_base = &data_source_.desc_buffer().at(desc_offset);
    sge[num_sge].iov_len = sizeof(fles::MicrosliceDescriptor) * desc_length;
    assert(mr_desc_ != nullptr);
    descs[num_sge++] = fi_mr_desc(mr_desc_);
    // sge[num_sge++].lkey = mr_desc_->lkey;
  } else {
    // two chunks
    sge[num_sge].iov_base = &data_source_.desc_buffer().at(desc_offset);
    sge[num_sge].iov_len =
        sizeof(fles::MicrosliceDescriptor) *
        (data_source_.desc_buffer().size() -
         (desc_offset & data_source_.desc_buffer().size_mask()));
    // sge[num_sge++].lkey = mr_desc_->lkey;
    descs[num_sge++] = fi_mr_desc(mr_desc_);
    sge[num_sge].iov_base = data_source_.desc_buffer().ptr();
    sge[num_sge].iov_len =
        sizeof(fles::MicrosliceDescriptor) *
        (desc_length - data_source_.desc_buffer().size() +
         (desc_offset & data_source_.desc_buffer().size_mask()));
    // sge[num_sge++].lkey = mr_desc_->lkey;
    descs[num_sge++] = fi_mr_desc(mr_desc_);
  }
  // int num_desc_sge = num_sge;
  // data
  if (data_length == 0) {
    // zero chunks
  } else if ((data_offset & data_source_.data_buffer().size_mask()) <=
             ((data_offset + data_length - 1) &
              data_source_.data_buffer().size_mask())) {
    // one chunk
    sge[num_sge].iov_base = &data_source_.data_buffer().at(data_offset);
    sge[num_sge].iov_len = data_length;
    descs[num_sge++] = fi_mr_desc(mr_data_);
  } else {
    // two chunks
    sge[num_sge].iov_base = &data_source_.data_buffer().at(data_offset);
    sge[num_sge].iov_len =
        data_source_.data_buffer().size() -
        (data_offset & data_source_.data_buffer().size_mask());
    descs[num_sge++] = fi_mr_desc(mr_data_);
    sge[num_sge].iov_base = data_source_.data_buffer().ptr();
    sge[num_sge].iov_len =
        data_length - data_source_.data_buffer().size() +
        (data_offset & data_source_.data_buffer().size_mask());
    descs[num_sge++] = fi_mr_desc(mr_data_);
  }

  return conn_[cn]->send_data(sge, descs, num_sge, timeslice, desc_length,
                              data_length, skip);
}

void InputChannelSender::on_completion(uint64_t wr_id) {
  switch (wr_id & 0xFF) {
  case ID_WRITE_DESC:
  case ID_WRITE_DATA:
  case ID_WRITE_DATA_WRAP: {
    uint64_t ts = wr_id >> 24;

    int cn = (wr_id >> 8) & 0xFFFF;
    InputSchedulerOrchestrator::mark_timeslice_rdma_write_acked(cn, ts);
    conn_[cn]->on_complete_write();

    if (false) {
      L_(info) << "[i" << input_index_ << "] "
               << "write timeslice " << ts << " to " << cn
               << " complete, now: acked_data_=" << acked_data_
               << " acked_desc_=" << acked_desc_;
    }
  } break;

  case ID_RECEIVE_STATUS: {
    int cn = wr_id >> 8;
    uint64_t last_desc = conn_[cn]->cn_ack_desc();
    bool was_done = conn_[cn]->done();
    conn_[cn]->on_complete_recv();
    uint64_t new_desc = conn_[cn]->cn_ack_desc();

    update_data_source(cn, last_desc, new_desc);
    InputSchedulerOrchestrator::mark_timeslices_acked(cn, new_desc);

    if (!connection_oriented_ && !conn_[cn]->get_partner_addr()) {
      conn_[cn]->set_partner_addr(av_);
      conn_[cn]->set_remote_info();
      on_connected(pd_);
      ++connected_;
      connected_indexes_.insert(cn);
    }
    if (conn_[cn]->request_abort_flag()) {
      abort_ = true;
    }
    if (!was_done && conn_[cn]->done()) {
      mark_connection_completed(cn);
    }
  } break;

  case ID_SEND_STATUS: {
    int cn = wr_id >> 8;
    conn_[cn]->on_complete_send();
  } break;

  case ID_HEARTBEAT_RECEIVE_STATUS: {
    int cn = wr_id >> 8;

    // TODO to be written in a better way
    InputSchedulerOrchestrator::update_data_source_desc(
        data_source_.get_write_index().desc);
    bool was_decision_considered = true, is_decision_considered = false;
    // Updating data source before re-arranging timeslice
    if (conn_[cn]->get_recv_heartbeat_message().failure_info.index !=
        ConstVariables::MINUS_ONE) {
      was_decision_considered =
          InputSchedulerOrchestrator::is_decision_considered(
              conn_[cn]->get_recv_heartbeat_message().failure_info.index);
    }

    conn_[cn]->on_complete_heartbeat_recv();

    is_decision_considered = InputSchedulerOrchestrator::is_decision_considered(
        conn_[cn]->get_recv_heartbeat_message().failure_info.index);
    // update the cn_wp after performing timeslice re-arrangment
    if (conn_[cn]->get_recv_heartbeat_message().failure_info.index !=
            ConstVariables::MINUS_ONE &&
        !was_decision_considered && is_decision_considered) {

      LibfabricBarrier::get_instance()->deactive_endpoint(
          conn_[cn]->get_recv_heartbeat_message().failure_info.index);
      uint64_t last_desc =
          conn_[conn_[cn]->get_recv_heartbeat_message().failure_info.index]
              ->cn_ack_desc();
      update_data_source(
          conn_[cn]->get_recv_heartbeat_message().failure_info.index, last_desc,
          conn_[cn]
              ->get_recv_heartbeat_message()
              .failure_info.last_completed_desc);

      for (auto& conn : conn_) {
        if (!InputSchedulerOrchestrator::is_connection_timed_out(
                conn->index())) {
          conn->update_cn_wp_after_failure_action(
              conn_[cn]->get_recv_heartbeat_message().failure_info.index);
        }
      }
      mark_connection_completed(
          conn_[cn]->get_recv_heartbeat_message().failure_info.index);
    }
  } break;

  case ID_HEARTBEAT_SEND_STATUS: {
    int cn = wr_id >> 8;
    conn_[cn]->on_complete_heartbeat_send();
  } break;

  default:
    L_(fatal) << "[i" << input_index_ << "] "
              << "wc for unknown wr_id=" << (wr_id & 0xFF);
    throw LibfabricException("wc for unknown wr_id");
  }
}

void InputChannelSender::update_compute_schedulers() {
  for (auto& c : conn_) {
    c->ack_complete_interval_info();
  }
}

void InputChannelSender::update_data_source(uint32_t compute_index,
                                            uint64_t old_desc,
                                            uint64_t new_desc) {
  for (uint64_t desc = old_desc + 1; desc <= new_desc; ++desc) {
    uint64_t ts = InputSchedulerOrchestrator::get_timeslice_by_descriptor(
        compute_index, desc);
    uint64_t acked_ts = (acked_desc_ - start_index_desc_) / timeslice_size_;
    assert(ts != ConstVariables::MINUS_ONE);
    if (ts != acked_ts) {
      // transmission has been reordered, store completion information
      ack_.at(ts) = ts;
    } else {
      // completion is for earliest pending timeslice, update indices
      do {
        ++acked_ts;
      } while (ack_.at(acked_ts) > ts);

      // TODO Invalid when timeslices are not fixed in size
      acked_desc_ = acked_ts * timeslice_size_ + start_index_desc_;
      acked_data_ = data_source_.desc_buffer().at(acked_desc_ - 1).offset +
                    data_source_.desc_buffer().at(acked_desc_ - 1).size;
      if (acked_data_ >= cached_acked_data_ + min_acked_data_ ||
          acked_desc_ >= cached_acked_desc_ + min_acked_desc_) {
        cached_acked_data_ = acked_data_;
        cached_acked_desc_ = acked_desc_;
        data_source_.set_read_index({cached_acked_desc_, cached_acked_data_});
      }
    }
  }
}

void InputChannelSender::mark_connection_completed(uint32_t cn) {
  conn_[cn]->mark_done();
  ++connections_done_;
  all_done_ = (connections_done_ == conn_.size());
  if (!connection_oriented_) {
    on_disconnected(nullptr, cn);
  }
  L_(warning) << "[i" << input_index_ << "] "
              << "ID_RECEIVE_STATUS final for id " << cn
              << " all_done=" << all_done_ << " done " << connections_done_;
}
} // namespace tl_libfabric
