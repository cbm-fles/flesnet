// Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBuilder.hpp"
#include "InputNodeInfo.hpp"
#include "RequestIdentifier.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceWorkItem.hpp"
#include <log.hpp>

TimesliceBuilder::TimesliceBuilder(
    uint64_t compute_index, TimesliceBuffer& timeslice_buffer,
    unsigned short service, uint32_t num_input_nodes, uint32_t timeslice_size,
    volatile sig_atomic_t* signal_status, bool drop)
    : compute_index_(compute_index), timeslice_buffer_(timeslice_buffer),
      service_(service), num_input_nodes_(num_input_nodes),
      timeslice_size_(timeslice_size),
      ack_(timeslice_buffer_.get_desc_size_exp()),
      signal_status_(signal_status), drop_(drop)
{
    assert(timeslice_buffer_.get_num_input_nodes() == num_input_nodes);
}

TimesliceBuilder::~TimesliceBuilder() {}

void TimesliceBuilder::report_status()
{
    constexpr auto interval = std::chrono::seconds(1);

    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

    L_(debug) << "[c" << compute_index_ << "] " << completely_written_
              << " completely written, " << acked_ << " acked";

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
    }

    scheduler_.add(std::bind(&TimesliceBuilder::report_status, this),
                   now + interval);
}

void TimesliceBuilder::request_abort()
{
    L_(info) << "[c" << compute_index_ << "] "
             << "request abort";

    for (auto& connection : conn_) {
        connection->request_abort();
    }
}

/// The thread main function.
void TimesliceBuilder::operator()()
{
    try {
        // set_cpu(0);

        accept(service_, num_input_nodes_);
        while (connected_ != num_input_nodes_) {
            poll_cm_events();
        }

        time_begin_ = std::chrono::high_resolution_clock::now();

        report_status();
        while (!all_done_ || connected_ != 0) {
            if (!all_done_) {
                poll_completion();
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

        summary();
    } catch (std::exception& e) {
        L_(error) << "exception in TimesliceBuilder: " << e.what();
    }
}

void TimesliceBuilder::on_connect_request(struct rdma_cm_event* event)
{
    if (!pd_)
        init_context(event->id->verbs);

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
void TimesliceBuilder::on_completion(const struct ibv_wc& wc)
{
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

            for (uint64_t tpos = completely_written_;
                 tpos < new_completely_written; ++tpos) {
                if (!drop_) {
                    uint64_t ts_index = UINT64_MAX;
                    if (conn_.size() > 0) {
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

void TimesliceBuilder::poll_ts_completion()
{
    fles::TimesliceCompletion c;
    if (!timeslice_buffer_.try_receive_completion(c))
        return;
    if (c.ts_pos == acked_) {
        do
            ++acked_;
        while (ack_.at(acked_) > c.ts_pos);
        for (auto& connection : conn_)
            connection->inc_ack_pointers(acked_);
    } else
        ack_.at(c.ts_pos) = c.ts_pos;
}
