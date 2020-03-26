// Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBuilder.hpp"
#include "InputNodeInfo.hpp"
#include "RequestIdentifier.hpp"
#include "System.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceWorkItem.hpp"
#include "log.hpp"
#include <algorithm>
#include <limits>

TimesliceBuilder::TimesliceBuilder(uint64_t compute_index,
                                   TimesliceBuffer& timeslice_buffer,
                                   unsigned short service,
                                   uint32_t num_input_nodes,
                                   uint32_t timeslice_size,
                                   volatile sig_atomic_t* signal_status,
                                   bool drop,
                                   const std::string& monitor_uri)
    : compute_index_(compute_index), timeslice_buffer_(timeslice_buffer),
      service_(service), num_input_nodes_(num_input_nodes),
      timeslice_size_(timeslice_size),
      ack_(timeslice_buffer_.get_desc_size_exp()),
      signal_status_(signal_status), drop_(drop) {
  assert(timeslice_buffer_.get_num_input_nodes() == num_input_nodes);

  if (!monitor_uri.empty()) {
    try {
      monitor_client_ = std::unique_ptr<web::http::client::http_client>(
          new web::http::client::http_client(monitor_uri));
    } catch (std::exception& e) {
      L_(error) << "cannot connect to monitoring at " << monitor_uri << ": "
                << e.what();
    }
  }
  hostname_ = fles::system::current_hostname();

  previous_recv_buffer_status_data_.resize(num_input_nodes);
  previous_recv_buffer_status_desc_.resize(num_input_nodes);
}

TimesliceBuilder::~TimesliceBuilder() = default;

void TimesliceBuilder::report_status() {
  constexpr auto interval = std::chrono::seconds(1);

  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  L_(debug) << "[c" << compute_index_ << "] " << completely_written_
            << " completely written, " << acked_ << " acked";

  std::string measurement;

  double total_rate_desc = 0.;
  double total_rate_data = 0.;

  float min_used_desc = std::numeric_limits<float>::max();
  float min_used_data = std::numeric_limits<float>::max();
  float min_free_desc = std::numeric_limits<float>::max();
  float min_free_data = std::numeric_limits<float>::max();
  float min_freeing_plus_free_desc = std::numeric_limits<float>::max();
  float min_freeing_plus_free_data = std::numeric_limits<float>::max();

  for (auto& c : conn_) {
    auto status_desc = c->buffer_status_desc();
    auto status_data = c->buffer_status_data();

    min_used_desc =
        std::min(min_used_desc, status_desc.percentage(status_desc.used()));
    min_used_data =
        std::min(min_used_data, status_data.percentage(status_data.used()));
    min_free_desc =
        std::min(min_free_desc, status_desc.percentage(status_desc.unused()));
    min_free_data =
        std::min(min_free_data, status_data.percentage(status_data.unused()));
    min_freeing_plus_free_desc =
        std::min(min_freeing_plus_free_desc,
                 status_desc.percentage(status_desc.freeing()) +
                     status_desc.percentage(status_desc.unused()));
    min_freeing_plus_free_data =
        std::min(min_freeing_plus_free_data,
                 status_data.percentage(status_data.freeing()) +
                     status_data.percentage(status_data.unused()));

    double delta_t =
        std::chrono::duration<double, std::chrono::seconds::period>(
            status_desc.time -
            previous_recv_buffer_status_desc_.at(c->index()).time)
            .count();
    double rate_desc =
        static_cast<double>(
            status_desc.received -
            previous_recv_buffer_status_desc_.at(c->index()).received) /
        delta_t;
    double rate_data =
        static_cast<double>(
            status_data.received -
            previous_recv_buffer_status_data_.at(c->index()).received) /
        delta_t;
    total_rate_desc += rate_desc;
    total_rate_data += rate_data;

    L_(debug) << "[c" << compute_index_ << "] desc "
              << status_desc.percentages() << " (used..free) | "
              << human_readable_count(status_desc.acked, true, "")
              << " timeslices";
    L_(debug) << "[c" << compute_index_ << "] data "
              << status_data.percentages() << " (used..free) | "
              << human_readable_count(status_data.acked, true);
    L_(debug) << "[c" << compute_index_ << "_" << c->index() << "] |"
              << bar_graph(status_data.vector(), "#._", 20) << "|"
              << bar_graph(status_desc.vector(), "#._", 10) << "| "
              << human_readable_count(rate_data, true, "B/s") << " ("
              << human_readable_count(rate_desc, true, "Hz") << ")";

    if (monitor_client_) {
      measurement += "recv_buffer_status,host=" + hostname_ +
                     ",output_index=" + std::to_string(compute_index_) +
                     ",input_index=" + std::to_string(c->index()) +
                     " data_used=" + std::to_string(status_data.used()) +
                     "i,data_freeing=" + std::to_string(status_data.freeing()) +
                     "i,data_free=" + std::to_string(status_data.unused()) +
                     "i,data_rate=" + std::to_string(rate_data) +
                     ",desc_used=" + std::to_string(status_desc.used()) +
                     "i,desc_freeing=" + std::to_string(status_desc.freeing()) +
                     "i,desc_free=" + std::to_string(status_desc.unused()) +
                     "i,desc_rate=" + std::to_string(rate_desc) + "\n";
    }

    previous_recv_buffer_status_data_.at(c->index()) = status_data;
    previous_recv_buffer_status_desc_.at(c->index()) = status_desc;
  }

  double min_freeing_desc = min_freeing_plus_free_desc - min_free_desc;
  double min_freeing_data = min_freeing_plus_free_data - min_free_data;
  double mixed_desc = 1. - min_used_desc - min_free_desc - min_freeing_desc;
  double mixed_data = 1. - min_used_data - min_free_data - min_freeing_data;

  auto total_status_data = std::vector<double>{min_used_data, mixed_data,
                                               min_freeing_data, min_free_data};
  auto total_status_desc = std::vector<double>{min_used_desc, mixed_desc,
                                               min_freeing_desc, min_free_desc};

  L_(status) << "[c" << compute_index_ << "]   |"
             << bar_graph(total_status_data, "#=._", 20) << "|"
             << bar_graph(total_status_desc, "#=._", 10) << "| "
             << human_readable_count(total_rate_data, true, "B/s") << " ("
             << human_readable_count(total_rate_desc, true, "Hz") << ")";

  measurement +=
      "timeslice_buffer_status,host=" + hostname_ +
      ",output_index=" + std::to_string(compute_index_) +
      " data_used=" + std::to_string(min_used_data) +
      ",data_mixed=" + std::to_string(mixed_data) +
      ",data_freeing=" + std::to_string(min_freeing_data) +
      ",data_free=" + std::to_string(min_free_data) +
      ",data_rate=" + std::to_string(total_rate_data) +
      ",desc_used=" + std::to_string(min_used_desc) +
      ",desc_mixed=" + std::to_string(mixed_desc) +
      ",desc_freeing=" + std::to_string(min_freeing_desc) +
      ",desc_free=" + std::to_string(min_free_desc) +
      ",desc_rate=" + std::to_string(total_rate_desc) +
      ",work_items=" + std::to_string(timeslice_buffer_.get_num_work_items()) +
      "i\n";

  if (monitor_client_) {
    // if task is pending and done, clean it up
    if (monitor_task_) {
      if (monitor_task_->is_done()) {
        try {
          monitor_task_->get();
        } catch (std::exception& e) {
          L_(error) << "monitor task failed: " << e.what();
        }
        monitor_task_ = nullptr;
      } else {
        L_(warning) << "monitor task is taking longer than expected";
      }
    }

    if (!monitor_task_) {
      auto task =
          monitor_client_
              ->request(web::http::methods::POST,
                        "/write?db=flesnet_status&precision=s", measurement)
              .then([](const web::http::http_response& response) {
                if (response.status_code() != 204) {
                  L_(error)
                      << "Monitoring client received response status code "
                      << response.status_code() << ": "
                      << response.extract_string().get();
                }
              });

      monitor_task_ =
          std::unique_ptr<pplx::task<void>>(new pplx::task<void>(task));
    }
  }

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

/// The thread main function.
void TimesliceBuilder::operator()() {
  try {
    // set_cpu(0);

    accept(service_, num_input_nodes_);
    while (connected_ != num_input_nodes_) {
      poll_cm_events();
    }
    L_(info) << "[c" << compute_index_ << "] "
             << "connection to input nodes established";

    time_begin_ = std::chrono::high_resolution_clock::now();

    report_status();
    while (!all_done_ || connected_ != 0 || timewait_ != 0) {
      if (!all_done_) {
        poll_completion();
        poll_ts_completion();
      }
      if (connected_ != 0 || timewait_ != 0) {
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

    summary();
  } catch (std::exception& e) {
    L_(error) << "exception in TimesliceBuilder: " << e.what();
  }
}

void TimesliceBuilder::on_connect_request(struct rdma_cm_event* event) {
  if (pd_ == nullptr) {
    init_context(event->id->verbs);
  }

  assert(event->param.conn.private_data_len >= sizeof(InputNodeInfo));
  InputNodeInfo remote_info =
      *reinterpret_cast<const InputNodeInfo*>(event->param.conn.private_data);

  uint_fast16_t index = remote_info.index;
  assert(index < conn_.size() && conn_.at(index) == nullptr);

  std::unique_ptr<ComputeNodeConnection> conn(new ComputeNodeConnection(
      ec_, index, compute_index_, event->id, remote_info,
      timeslice_buffer_.get_data_ptr(index),
      timeslice_buffer_.get_data_size_exp(),
      timeslice_buffer_.get_desc_ptr(index),
      timeslice_buffer_.get_desc_size_exp()));
  conn_.at(index) = std::move(conn);

  conn_.at(index)->on_connect_request(event, pd_, cq_);
}

/// Completion notification event dispatcher. Called by the event loop.
void TimesliceBuilder::on_completion(const struct ibv_wc& wc) {
  size_t in = wc.wr_id >> 8;
  assert(in < conn_.size());
  switch (wc.wr_id & 0xFF) {

  case ID_SEND_STATUS:
    if (false) {
      L_(trace) << "[c" << compute_index_ << "] "
                << "[" << in << "] "
                << "COMPLETE SEND status message";
    }
    conn_[in]->on_complete_send();
    break;

  case ID_SEND_FINALIZE: {
    if (!conn_[in]->abort_flag()) {
      assert(timeslice_buffer_.get_num_work_items() == 0);
      assert(timeslice_buffer_.get_num_completions() == 0);
    }
    conn_[in]->on_complete_send();
    conn_[in]->on_complete_send_finalize();
    ++connections_done_;
    all_done_ = (connections_done_ == conn_.size());
    L_(debug) << "[c" << compute_index_ << "] "
              << "SEND FINALIZE complete for id " << in
              << " all_done=" << all_done_;
  } break;

  case ID_RECEIVE_STATUS: {
    conn_[in]->on_complete_recv();
    if (connected_ == conn_.size() && in == red_lantern_) {
      auto new_red_lantern = std::min_element(
          std::begin(conn_), std::end(conn_),
          [](const std::unique_ptr<ComputeNodeConnection>& v1,
             const std::unique_ptr<ComputeNodeConnection>& v2) {
            return v1->cn_wp().desc < v2->cn_wp().desc;
          });

      uint64_t new_completely_written = (*new_red_lantern)->cn_wp().desc;
      red_lantern_ = std::distance(std::begin(conn_), new_red_lantern);

      for (uint64_t tpos = completely_written_; tpos < new_completely_written;
           ++tpos) {
        if (!drop_) {
          uint64_t ts_index = UINT64_MAX;
          if (!conn_.empty()) {
            ts_index = timeslice_buffer_.get_desc(0, tpos).ts_num;
          }
          timeslice_buffer_.send_work_item(
              {{ts_index, tpos, timeslice_size_,
                static_cast<uint32_t>(conn_.size())},
               timeslice_buffer_.get_data_size_exp(),
               timeslice_buffer_.get_desc_size_exp()});
        } else {
          timeslice_buffer_.send_completion({tpos});
        }
      }

      completely_written_ = new_completely_written;
    }
  } break;

  default:
    throw InfinibandException("wc for unknown wr_id");
  }
}

void TimesliceBuilder::poll_ts_completion() {
  fles::TimesliceCompletion c;
  if (!timeslice_buffer_.try_receive_completion(c)) {
    return;
  }
  if (c.ts_pos == acked_) {
    do {
      ++acked_;
    } while (ack_.at(acked_) > c.ts_pos);
    for (auto& connection : conn_) {
      connection->inc_ack_pointers(acked_);
    }
  } else {
    ack_.at(c.ts_pos) = c.ts_pos;
  }
}
