// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "TimesliceBuilder.hpp"

namespace tl_libfabric {

TimesliceBuilder::TimesliceBuilder(
    uint64_t compute_index,
    TimesliceBuffer& timeslice_buffer,
    unsigned short service,
    uint32_t num_input_nodes,
    uint32_t timeslice_size,
    volatile sig_atomic_t* signal_status,
    bool drop,
    std::string local_node_name,
    uint32_t scheduler_history_size,
    uint32_t scheduler_interval_length,
    uint32_t scheduler_speedup_difference_percentage,
    uint32_t scheduler_speedup_percentage,
    uint32_t scheduler_speedup_interval_count,
    std::string log_directory,
    bool enable_logging)
    : ConnectionGroup(local_node_name), compute_index_(compute_index),
      timeslice_buffer_(timeslice_buffer), service_(service),
      num_input_nodes_(num_input_nodes), timeslice_size_(timeslice_size),
      ack_(timeslice_buffer_.get_desc_size_exp()),
      signal_status_(signal_status), local_node_name_(local_node_name),
      drop_(drop), log_directory_(log_directory) {
  listening_cq_ = nullptr;
  assert(timeslice_buffer_.get_num_input_nodes() == num_input_nodes);
  assert(not local_node_name_.empty());
  if (Provider::getInst()->is_connection_oriented()) {
    connection_oriented_ = true;
  } else {
    connection_oriented_ = false;
  }
  DDSchedulerOrchestrator::initialize(
      compute_index, num_input_nodes, ConstVariables::INIT_HEARTBEAT_TIMEOUT,
      ConstVariables::HEARTBEAT_TIMEOUT_HISTORY_SIZE,
      ConstVariables::HEARTBEAT_TIMEOUT_FACTOR,
      ConstVariables::HEARTBEAT_INACTIVE_FACTOR,
      ConstVariables::HEARTBEAT_INACTIVE_RETRY_COUNT, scheduler_history_size,
      scheduler_interval_length, scheduler_speedup_difference_percentage,
      scheduler_speedup_percentage, scheduler_speedup_interval_count,
      log_directory, enable_logging);
}

TimesliceBuilder::~TimesliceBuilder() {}

void TimesliceBuilder::report_status() {
  constexpr auto interval = std::chrono::seconds(1);

  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  L_(debug) << "[c" << compute_index_ << "] " << completely_written_
            << " completely written, " << acked_ << " acked";

  // LOGGING
  std::vector<double> buffer_percentage(conn_.size());
  //
  for (auto& c : conn_) {
    auto status_desc = c->buffer_status_desc();
    auto status_data = c->buffer_status_data();
    L_(debug) << "[c" << compute_index_ << "] desc "
              << status_desc.percentages() << " (used..free) | "
              << human_readable_count(status_desc.acked, true, "")
              << " timeslices";
    L_(debug) << "[c" << compute_index_ << "] data "
              << status_data.percentages() << " (used..free) | "
              << human_readable_count(status_data.acked, true);
    L_(info) << "[c" << compute_index_ << "_" << c->index() << "] |"
             << bar_graph(status_data.vector(), "#._", 20) << "|"
             << bar_graph(status_desc.vector(), "#._", 10) << "| ";

    // LOGGING
    buffer_percentage[c->index()] =
        std::stod(status_data.percentage_str(status_data.used()));
    //
  }
  // LOGGING
  int last_second = -1;
  if (!buffer_status_.empty())
    last_second = (--buffer_status_.end())->first;
  buffer_status_.insert(std::pair<uint64_t, std::vector<double>>(
      (last_second + 1), buffer_percentage));
  //

  scheduler_.add(std::bind(&TimesliceBuilder::report_status, this),
                 now + interval);
}

void TimesliceBuilder::request_abort() {

  L_(info) << "[c" << compute_index_ << "] "
           << "request abort";

  for (auto& connection : conn_) {
    connection->request_abort();
  }
}

void TimesliceBuilder::bootstrap_with_connections() {
  accept(local_node_name_, service_, num_input_nodes_);
  while (connected_ != num_input_nodes_) {
    poll_cm_events();
  }
}

// @todo duplicate code
void TimesliceBuilder::make_endpoint_named(struct fi_info* info,
                                           const std::string& hostname,
                                           const std::string& service,
                                           struct fid_ep** ep) {

  uint64_t requested_key = 0;
  int res;

  struct fi_info* info2 = nullptr;
  struct fi_info* hints = Provider::get_hints(
      info->ep_attr->type, info->fabric_attr->prov_name); // fi_dupinfo(info);

  int err = fi_getinfo(FIVERSION, hostname.c_str(), service.c_str(), FI_SOURCE,
                       hints, &info2);
  if (err != 0) {
    L_(fatal) << "fi_getinfo failed in make_endpoint named: " << err << "="
              << fi_strerror(-err);
    throw LibfabricException("fi_getinfo failed in make_endpoint named");
  }

  fi_freeinfo(hints);

  // private cq for listening ep
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(cq_attr));
  cq_attr.size = num_cqe_;
  cq_attr.flags = 0;
  cq_attr.format = FI_CQ_FORMAT_TAGGED;
  cq_attr.wait_obj = FI_WAIT_NONE;
  cq_attr.signaling_vector = Provider::vector++; // ??
  cq_attr.wait_cond = FI_CQ_COND_NONE;
  cq_attr.wait_set = nullptr;
  res = fi_cq_open(pd_, &cq_attr, &listening_cq_, nullptr);
  if (listening_cq_ == nullptr) {
    L_(fatal) << "fi_cq_open failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_cq_open failed");
  }

  err = fi_endpoint(pd_, info2, ep, this);
  if (err != 0) {
    L_(fatal) << "fi_endpoint failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_endpoint failed");
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  if (Provider::getInst()->has_eq_at_eps()) {
    err = fi_ep_bind(*ep, (::fid_t)eq_, 0);
    if (err != 0) {
      L_(fatal) << "fi_ep_bind failed (eq_): " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_ep_bind failed (eq_)");
    }
  }
  err = fi_ep_bind(*ep, (::fid_t)listening_cq_, FI_SEND | FI_RECV);
  if (err != 0) {
    L_(fatal) << "fi_ep_bind failed (cq): " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_ep_bind failed (cq)");
  }
  if (Provider::getInst()->has_av()) {
    err = fi_ep_bind(*ep, (fid_t)av_, 0);
    if (err != 0) {
      L_(fatal) << "fi_ep_bind failed (av): " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_ep_bind failed (av)");
    }
  }
#pragma GCC diagnostic pop
  err = fi_enable(*ep);
  if (err != 0) {
    L_(fatal) << "fi_enable failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_enable failed");
  }

  // register memory regions
  res =
      fi_mr_reg(pd_, &recv_connect_message_, sizeof(InputChannelStatusMessage),
                FI_RECV, 0, requested_key++, 0, &mr_recv_, nullptr);

  if (res != 0) {
    L_(fatal) << "fi_mr_reg failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_mr_reg failed");
  }

  if (mr_recv_ == nullptr) {
    throw LibfabricException(
        "registration of memory region failed in TimesliceBuilder");
  }
}

void TimesliceBuilder::bootstrap_wo_connections() {
  InputChannelStatusMessage recv_connect_message;
  struct fid_mr* mr_recv_connect = nullptr;
  struct fi_msg_tagged recv_msg_wr;
  struct iovec recv_sge = iovec();
  void* recv_wr_descs[1] = {nullptr};

  // domain, cq, av
  init_context(Provider::getInst()->get_info(), {}, {});
  LibfabricBarrier::create_barrier_instance(compute_index_, pd_, true);

  // listening endpoint with private cq
  make_endpoint_named(Provider::getInst()->get_info(), local_node_name_,
                      std::to_string(service_), &ep_);

  // setup connection objects
  for (size_t index = 0; index < conn_.size(); index++) {
    uint8_t* data_ptr = timeslice_buffer_.get_data_ptr(index);
    fles::TimesliceComponentDescriptor* desc_ptr =
        timeslice_buffer_.get_desc_ptr(index);

    std::unique_ptr<ComputeNodeConnection> conn(new ComputeNodeConnection(
        eq_, pd_, completion_queue(index), av_, index, compute_index_, data_ptr,
        timeslice_buffer_.get_data_size_exp(), desc_ptr,
        timeslice_buffer_.get_desc_size_exp()));
    conn->setup_mr(pd_);
    conn->setup();
    conn_.at(index) = std::move(conn);
  }

  // register memory regions
  int err = fi_mr_reg(pd_, &recv_connect_message, sizeof(recv_connect_message),
                      FI_RECV | FI_TAGGED, 0, Provider::requested_key++, 0,
                      &mr_recv_connect, nullptr);
  if (err != 0) {
    L_(fatal) << "fi_mr_reg failed for recv msg in compute-buffer: " << err
              << "=" << fi_strerror(-err);
    throw LibfabricException("fi_mr_reg failed for recv msg in compute-buffer");
  }

  // prepare recv message
  recv_sge.iov_base = &recv_connect_message;
  recv_sge.iov_len = sizeof(recv_connect_message);

  recv_wr_descs[0] = fi_mr_desc(mr_recv_);

  recv_msg_wr.msg_iov = &recv_sge;
  recv_msg_wr.desc = recv_wr_descs;
  recv_msg_wr.iov_count = 1;
  recv_msg_wr.addr = FI_ADDR_UNSPEC;
  recv_msg_wr.context = LibfabricContextPool::getInst()->getContext();
  recv_msg_wr.data = 0;
  recv_msg_wr.tag = ConstVariables::STATUS_MESSAGE_TAG;

  err = fi_trecvmsg(ep_, &recv_msg_wr, FI_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_recvmsg failed: " << strerror(err);
    throw LibfabricException("fi_recvmsg failed");
  }

  // wait for messages from InputChannelSenders
  const int ne_max = 1; // the ne_max must be always 1 because there is ONLY
                        // ONE mr registered variable for receiving messages

  struct fi_cq_tagged_entry wc[ne_max];
  int ne;

  while (connected_senders_.size() != num_input_nodes_) {

    while ((ne = fi_cq_read(listening_cq_, &wc, ne_max))) {
      if ((ne < 0) && (ne != -FI_EAGAIN)) {
        L_(fatal) << "fi_cq_read failed: " << ne << "=" << fi_strerror(-ne);
        throw LibfabricException("fi_cq_read failed");
      }
      if (ne == FI_SEND) {
        continue;
      }

      if (ne == -FI_EAGAIN) {
        break;
      }

      L_(debug) << "got " << ne << " events";
      for (int i = 0; i < ne; ++i) {
        fi_addr_t connection_addr;
        // when connect message:
        //            add address to av and set fi_addr_t from av on
        //            conn-object
        assert(recv_connect_message.connect == true);
        if (connected_senders_.find(recv_connect_message.info.index) !=
            connected_senders_.end()) {
          conn_.at(recv_connect_message.info.index)->send_ep_addr();
          continue;
        }
        int res = fi_av_insert(av_, &recv_connect_message.my_address, 1,
                               &connection_addr, 0, NULL);
        assert(res == 1);
        // conn_.at(recv_connect_message.info.index)->on_complete_recv();
        conn_.at(recv_connect_message.info.index)
            ->set_partner_addr(connection_addr);
        conn_.at(recv_connect_message.info.index)
            ->set_remote_info(recv_connect_message.info);
        conn_.at(recv_connect_message.info.index)->send_ep_addr();
        connected_senders_.insert(recv_connect_message.info.index);
        ++connected_;
        err = fi_trecvmsg(ep_, &recv_msg_wr, FI_COMPLETION);
      }
    }
    if (err != 0) {
      L_(fatal) << "fi_recvmsg failed: " << strerror(err);
      throw LibfabricException("fi_recvmsg failed");
    }
  }
  LibfabricContextPool::getInst()->releaseContext(
      static_cast<struct fi_custom_context*>(recv_msg_wr.context));
}

/// The thread main function.
void TimesliceBuilder::operator()() {
  try {
    // set_cpu(0);

    if (connection_oriented_) {
      bootstrap_with_connections();
    } else {
      conn_.resize(num_input_nodes_);
      bootstrap_wo_connections();
    }

    LibfabricBarrier::get_instance()->call_barrier();
    
    time_begin_ = std::chrono::high_resolution_clock::now();
    DDSchedulerOrchestrator::set_begin_time(time_begin_);

    sync_buffer_positions();
    report_status();
    sync_heartbeat();
    while (!all_done_ || connected_ != 0) {
      if (!all_done_) {
        poll_completion();
        process_completed_timeslices();
        poll_ts_completion();
      }
      if (connected_ != 0) {
        poll_cm_events();
      }
      scheduler_.timer();
      if (*signal_status_ != 0) {
        *signal_status_ = 0;
        request_abort();
      }
    }

    time_end_ = std::chrono::high_resolution_clock::now();

    timeslice_buffer_.send_end_work_item();
    timeslice_buffer_.send_end_completion();

    DDSchedulerOrchestrator::generate_log_files();

    build_time_file();
    summary();
  } catch (std::exception& e) {
    L_(error) << "exception in TimesliceBuilder: " << e.what();
  }
}

void TimesliceBuilder::build_time_file() {

  if (true) {
    std::ofstream log_file;
    log_file.open(log_directory_ + "/" + std::to_string(compute_index_) +
                  ".compute.buffer_status.out");

    log_file << std::setw(25) << "Second" << std::setw(25);
    for (uint32_t i = 0; i < conn_.size(); i++)
      log_file << "Conn_" << i << std::setw(25);
    log_file << "\n";

    std::map<uint64_t, std::vector<double>>::iterator it =
        buffer_status_.begin();
    while (it != buffer_status_.end()) {
      log_file << std::setw(25) << it->first << std::setw(25);
      for (uint32_t i = 0; i < it->second.size(); i++)
        log_file << it->second[i] << std::setw(25);
      log_file << "\n";
      ++it;
    }

    log_file.flush();
    log_file.close();
  }
}

void TimesliceBuilder::on_connect_request(struct fi_eq_cm_entry* event,
                                          size_t private_data_len) {

  if (pd_ == nullptr) {
    init_context(event->info, {}, {});
    LibfabricBarrier::create_barrier_instance(compute_index_, pd_, true);
  }

  assert(private_data_len >= sizeof(InputNodeInfo));
  InputNodeInfo remote_info;
  /* pacify strict-aliasing rules */
  memcpy(&remote_info, event->data, sizeof(remote_info));

  uint_fast16_t index = remote_info.index;
  assert(index < conn_.size() && conn_.at(index) == nullptr);

  std::unique_ptr<ComputeNodeConnection> conn(
      new ComputeNodeConnection(eq_, index, compute_index_, remote_info,
                                timeslice_buffer_.get_data_ptr(index),
                                timeslice_buffer_.get_data_size_exp(),
                                timeslice_buffer_.get_desc_ptr(index),
                                timeslice_buffer_.get_desc_size_exp()));
  conn_.at(index) = std::move(conn);

  conn_.at(index)->on_connect_request(event, pd_, completion_queue(index));
}

/// Completion notification event dispatcher. Called by the event loop.
void TimesliceBuilder::on_completion(uint64_t wr_id) {
  size_t in = wr_id >> 8;
  assert(in < conn_.size());
  switch (wr_id & 0xFF) {
  case ID_SEND_STATUS:
    if (false) {
      L_(info) << "[c" << compute_index_ << "] "
               << "[" << in << "] "
               << "COMPLETE SEND status message";
    }
    conn_[in]->on_complete_send();
    break;

  case ID_SEND_FINALIZE: {
    on_send_finalize_event(in);
  } break;

  case ID_RECEIVE_STATUS: {
    conn_[in]->on_complete_recv();
  } break;

  case ID_HEARTBEAT_RECEIVE_STATUS: {
    int cn = wr_id >> 8;
    on_recv_heartbeat_event(cn);

  } break;

  case ID_HEARTBEAT_SEND_STATUS: {
    int cn = wr_id >> 8;
    conn_[cn]->on_complete_heartbeat_send();
  } break;

  default:
    throw LibfabricException("wc for unknown wr_id");
  }
}

void TimesliceBuilder::poll_ts_completion() {
  while (1) {
    fles::TimesliceCompletion c;
    if (!timeslice_buffer_.try_receive_completion(c))
      return;
    if (c.ts_pos == acked_) {
      do
        ++acked_;
      while (ack_.at(acked_) > c.ts_pos);
      for (auto& connection : conn_) {
        // check timed out timeslice
        if (acked_ > connection->cn_wp().desc)
          continue;
        connection->inc_ack_pointers(acked_);
      }
    } else
      ack_.at(c.ts_pos) = c.ts_pos;
  }
}

bool TimesliceBuilder::check_complete_timeslices(uint64_t ts_pos) {
  bool all_received = true;
  for (uint32_t indx = 0; indx < conn_.size(); indx++) {
    const fles::TimesliceComponentDescriptor& acked_ts =
        timeslice_buffer_.get_desc(indx, ts_pos);
    L_(debug) << "[process_pending_complete_timeslices] desc = " << ts_pos
              << ", acked_ts.size = " << acked_ts.size
              << ", acked_ts.offset = " << acked_ts.offset
              << ", acked_ts.num_microslices = " << acked_ts.num_microslices
              << ", acked_ts.ts_num = " << acked_ts.ts_num
              << ", (acked_ts.offset + acked_ts.size) = "
              << (acked_ts.offset + acked_ts.size)
              << ", conn_[indx]->cn_ack().data = "
              << conn_[indx]->cn_ack().data;
    if (acked_ts.num_microslices == ConstVariables::ZERO ||
        acked_ts.size == ConstVariables::ZERO ||
        (acked_ts.offset + acked_ts.size) < conn_[indx]->cn_ack().data) {
      all_received = false;
      break;
    }
  }
  return all_received;
}

void TimesliceBuilder::process_completed_timeslices() {
  if (connected_ != conn_.size() ||
      !DDSchedulerOrchestrator::is_all_failure_decisions_acked()) {
    return;
  }

  DDSchedulerOrchestrator::log_timeout_timeslice();
  uint64_t new_completely_written =
      DDSchedulerOrchestrator::get_last_ordered_completed_timeslice();
  if (new_completely_written == ConstVariables::MINUS_ONE ||
      new_completely_written < completely_written_)
    return;
  for (uint64_t ts_pos = completely_written_; ts_pos <= new_completely_written;
       ++ts_pos) {

    bool timed_out = DDSchedulerOrchestrator::is_timeslice_timed_out(ts_pos);
    // check whether all contributions are received if it is not timed out!
    if (!timed_out && !check_complete_timeslices(ts_pos)) {
      timed_out = true;
      L_(fatal) << "ts: " << ts_pos << " is not completely received yet ...";
    }
    if (!drop_ && !timed_out) {
      const fles::TimesliceComponentDescriptor& acked_ts =
          timeslice_buffer_.get_desc(0, ts_pos);
      uint64_t ts_index = acked_ts.ts_num;
      timeslice_buffer_.send_work_item({{ts_index, ts_pos, timeslice_size_,
                                         static_cast<uint32_t>(conn_.size())},
                                        timeslice_buffer_.get_data_size_exp(),
                                        timeslice_buffer_.get_desc_size_exp()});
    } else {
      timeslice_buffer_.send_completion({ts_pos});
    }
  }
  completely_written_ = new_completely_written + 1;
}

void TimesliceBuilder::sync_heartbeat() {
  check_missing_connections_failure_info();
  check_long_waiting_finalized_connections();
  check_inactive_connections();

  scheduler_.add(std::bind(&TimesliceBuilder::sync_heartbeat, this),
                 std::chrono::system_clock::now() + std::chrono::seconds(1));
}

void TimesliceBuilder::check_missing_connections_failure_info() {
  std::pair<uint32_t, std::set<uint32_t>> missing_info =
      DDSchedulerOrchestrator::retrieve_missing_info_from_connections();
  if (missing_info.first != ConstVariables::MINUS_ONE) {
    HeartbeatFailedNodeInfo* decision = new HeartbeatFailedNodeInfo();
    decision->index = missing_info.first;
    decision->last_completed_desc = ConstVariables::MINUS_ONE;
    decision->timeslice_trigger = ConstVariables::MINUS_ONE;
    std::set<uint32_t>::iterator it = missing_info.second.begin();
    while (it != missing_info.second.end()) {
      conn_[*it]->prepare_heartbeat(decision);
      ++it;
    }
    delete decision;
  }
}

void TimesliceBuilder::check_long_waiting_finalized_connections() {
  std::vector<uint32_t> long_waiting_finalize_conns =
      DDSchedulerOrchestrator::retrieve_long_waiting_finalized_connections();
  for (uint32_t i = 0; i < long_waiting_finalize_conns.size(); i++) {
    mark_connection_completed(long_waiting_finalize_conns[i]);
  }
}

void TimesliceBuilder::check_inactive_connections() {

  int32_t failed_connection =
      DDSchedulerOrchestrator::get_new_timeout_connection();
  if (failed_connection == -1) { // Check inactive connections
    send_heartbeat_to_inactive_connections();
  } else { // mark connection as completed
    DDSchedulerOrchestrator::mark_connection_timed_out(failed_connection);
    if (DDSchedulerOrchestrator::get_timeout_connection_count() ==
        conn_.size()) {
      for (auto& conn : conn_) {
        mark_connection_completed(conn->index());
      }
    }
  }
}

void TimesliceBuilder::mark_connection_completed(uint32_t conn_id) {
  if (conn_[conn_id]->done())
    return;

  DDSchedulerOrchestrator::log_finalize_connection(conn_id, true);
  conn_[conn_id]->on_complete_send_finalize();
  ++connections_done_;
  all_done_ = (connections_done_ == conn_.size());
  if (!connection_oriented_) {
    on_disconnected(nullptr, conn_id);
  } else {
    conn_[conn_id]->disconnect();
  }
}

void TimesliceBuilder::on_send_finalize_event(uint32_t conn_id) {
  if (!conn_[conn_id]->abort_flag()) {
    assert(timeslice_buffer_.get_num_work_items() == 0);
    assert(timeslice_buffer_.get_num_completions() == 0);
  }
  conn_[conn_id]->on_complete_send();
  if (!conn_[conn_id]->done()) {
    mark_connection_completed(conn_id);
  }
  L_(debug) << "[c" << compute_index_ << "] "
            << "SEND FINALIZE complete for id " << conn_id
            << " all_done=" << all_done_;
}

void TimesliceBuilder::on_recv_heartbeat_event(uint32_t conn_id) {
  const HeartbeatMessage recv_heartbeat_message =
      conn_[conn_id]->recv_heartbeat_message();
  bool before_failed_node_decision_taken = false,
       after_failed_node_decision_taken = false;
  if (recv_heartbeat_message.failure_info.index != ConstVariables::MINUS_ONE)
    before_failed_node_decision_taken =
        DDSchedulerOrchestrator::is_failed_node_decision_ready(
            recv_heartbeat_message.failure_info.index);

  conn_[conn_id]->on_complete_heartbeat_recv();

  if (recv_heartbeat_message.failure_info.index != ConstVariables::MINUS_ONE)
    after_failed_node_decision_taken =
        DDSchedulerOrchestrator::is_failed_node_decision_ready(
            recv_heartbeat_message.failure_info.index);

  if (!before_failed_node_decision_taken && after_failed_node_decision_taken) {
    HeartbeatFailedNodeInfo* failednode_info =
        DDSchedulerOrchestrator::get_decision_of_failed_connection(
            recv_heartbeat_message.failure_info.index);
    assert(failednode_info != nullptr);
    for (auto& connection : conn_) {
      connection->prepare_heartbeat(failednode_info);
    }
  }
}

} // namespace tl_libfabric
