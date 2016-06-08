// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "ComputeBuffer.hpp"
#include "ChildProcessManager.hpp"
//#include "InputNodeInfo.hpp"
#include "RequestIdentifier.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceWorkItem.hpp"
#include <boost/algorithm/string.hpp>
//#include <boost/lexical_cast.hpp>
//#include <log.hpp>
//#include <random>

#include <valgrind/memcheck.h>

ComputeBuffer::ComputeBuffer(uint64_t compute_index,
                             uint32_t data_buffer_size_exp,
                             uint32_t desc_buffer_size_exp,
                             unsigned short service, uint32_t num_input_nodes,
                             uint32_t timeslice_size,
                             uint32_t processor_instances,
                             const std::string processor_executable,
                             volatile sig_atomic_t *signal_status,
                             std::string local_node_name)
    : compute_index_(compute_index),
      data_buffer_size_exp_(data_buffer_size_exp),
      desc_buffer_size_exp_(desc_buffer_size_exp), service_(service),
      num_input_nodes_(num_input_nodes), timeslice_size_(timeslice_size),
      processor_instances_(processor_instances),
      processor_executable_(processor_executable), ack_(desc_buffer_size_exp),
      signal_status_(signal_status), local_node_name_(local_node_name)
{
    std::random_device random_device;
    std::uniform_int_distribution<uint64_t> uint_distribution;
    uint64_t random_number = uint_distribution(random_device);
    shared_memory_identifier_ =
        "flesnet_" + boost::lexical_cast<std::string>(random_number);

    boost::interprocess::shared_memory_object::remove(
        (shared_memory_identifier_ + "data_").c_str());
    boost::interprocess::shared_memory_object::remove(
        (shared_memory_identifier_ + "desc_").c_str());

    std::unique_ptr<boost::interprocess::shared_memory_object> data_shm(
        new boost::interprocess::shared_memory_object(
            boost::interprocess::create_only,
            (shared_memory_identifier_ + "data_").c_str(),
            boost::interprocess::read_write));
    data_shm_ = std::move(data_shm);

    std::unique_ptr<boost::interprocess::shared_memory_object> desc_shm(
        new boost::interprocess::shared_memory_object(
            boost::interprocess::create_only,
            (shared_memory_identifier_ + "desc_").c_str(),
            boost::interprocess::read_write));
    desc_shm_ = std::move(desc_shm);

    std::size_t data_size =
        (UINT64_C(1) << data_buffer_size_exp_) * num_input_nodes_;
    assert(data_size != 0);
    data_shm_->truncate(data_size);

    std::size_t desc_buffer_size = (UINT64_C(1) << desc_buffer_size_exp_);

    std::size_t desc_size = desc_buffer_size * num_input_nodes_ *
                            sizeof(fles::TimesliceComponentDescriptor);
    assert(desc_size != 0);
    desc_shm_->truncate(desc_size);

    std::unique_ptr<boost::interprocess::mapped_region> data_region(
        new boost::interprocess::mapped_region(
            *data_shm_, boost::interprocess::read_write));
    data_region_ = std::move(data_region);

    std::unique_ptr<boost::interprocess::mapped_region> desc_region(
        new boost::interprocess::mapped_region(
            *desc_shm_, boost::interprocess::read_write));
    desc_region_ = std::move(desc_region);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    VALGRIND_MAKE_MEM_DEFINED(data_region_->get_address(),
                              data_region_->get_size());
    VALGRIND_MAKE_MEM_DEFINED(desc_region_->get_address(),
                              desc_region_->get_size());
#pragma GCC diagnostic pop

    boost::interprocess::message_queue::remove(
        (shared_memory_identifier_ + "work_items_").c_str());
    boost::interprocess::message_queue::remove(
        (shared_memory_identifier_ + "completions_").c_str());

    std::unique_ptr<boost::interprocess::message_queue> work_items_mq(
        new boost::interprocess::message_queue(
            boost::interprocess::create_only,
            (shared_memory_identifier_ + "work_items_").c_str(),
            desc_buffer_size, sizeof(fles::TimesliceWorkItem)));
    work_items_mq_ = std::move(work_items_mq);

    std::unique_ptr<boost::interprocess::message_queue> completions_mq(
        new boost::interprocess::message_queue(
            boost::interprocess::create_only,
            (shared_memory_identifier_ + "completions_").c_str(),
            desc_buffer_size, sizeof(fles::TimesliceCompletion)));
    completions_mq_ = std::move(completions_mq);
}

ComputeBuffer::~ComputeBuffer()
{
    boost::interprocess::shared_memory_object::remove(
        (shared_memory_identifier_ + "data_").c_str());
    boost::interprocess::shared_memory_object::remove(
        (shared_memory_identifier_ + "desc_").c_str());
    boost::interprocess::message_queue::remove(
        (shared_memory_identifier_ + "work_items_").c_str());
    boost::interprocess::message_queue::remove(
        (shared_memory_identifier_ + "completions_").c_str());
}

void ComputeBuffer::start_processes()
{
    assert(!processor_executable_.empty());
    for (uint_fast32_t i = 0; i < processor_instances_; ++i) {
        std::stringstream index;
        index << i;
        ChildProcess cp = ChildProcess();
        cp.owner = this;
        boost::split(cp.arg, processor_executable_, boost::is_any_of(" \t"),
                     boost::token_compress_on);
        cp.path = cp.arg.at(0);
        for (auto &arg : cp.arg) {
            boost::replace_all(arg, "%s", shared_memory_identifier_);
            boost::replace_all(arg, "%i", index.str());
        }
        ChildProcessManager::get().start_process(cp);
    }
}

void ComputeBuffer::report_status()
{
    constexpr auto interval = std::chrono::seconds(1);

    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

    L_(debug) << "[c" << compute_index_ << "] " << completely_written_
              << " completely written, " << acked_ << " acked";

    for (auto &c : conn_) {
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

    scheduler_.add(std::bind(&ComputeBuffer::report_status, this),
                   now + interval);
}

void ComputeBuffer::request_abort()
{
    L_(info) << "[c" << compute_index_ << "] "
             << "request abort";

    for (auto &connection : conn_) {
        connection->request_abort();
    }
}

/// The thread main function.
void ComputeBuffer::operator()()
{
    try
    {
        // set_cpu(0);

      accept(local_node_name_, service_, num_input_nodes_);
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

        ChildProcessManager::get().allow_stop_processes(this);

        for (uint_fast32_t i = 0; i < processor_instances_; ++i) {
            work_items_mq_->send(nullptr, 0, 0);
        }
        completions_mq_->send(nullptr, 0, 0);

        summary();
    }
    catch (std::exception &e)
    {
        L_(error) << "exception in ComputeBuffer: " << e.what();
    }
}

uint8_t *ComputeBuffer::get_data_ptr(uint_fast16_t index)
{
    return static_cast<uint8_t *>(data_region_->get_address()) +
           index * (UINT64_C(1) << data_buffer_size_exp_);
}

fles::TimesliceComponentDescriptor *
ComputeBuffer::get_desc_ptr(uint_fast16_t index)
{
    return reinterpret_cast<fles::TimesliceComponentDescriptor *>(
               desc_region_->get_address()) +
           index * (UINT64_C(1) << desc_buffer_size_exp_);
}

 uint8_t& ComputeBuffer::get_data(uint_fast16_t index, uint64_t offset)
{
    offset &= (UINT64_C(1) << data_buffer_size_exp_) - 1;
    return get_data_ptr(index)[offset];
}

fles::TimesliceComponentDescriptor &ComputeBuffer::get_desc(uint_fast16_t index,
                                                            uint64_t offset)
{
    offset &= (UINT64_C(1) << desc_buffer_size_exp_) - 1;
    return get_desc_ptr(index)[offset];
}

void ComputeBuffer::on_connect_request(struct fi_eq_cm_entry *event,
                                       size_t private_data_len)
{

  if(!pd_)
    init_context(event->info);

    assert(private_data_len >= sizeof(InputNodeInfo));
    InputNodeInfo remote_info =
      *reinterpret_cast<const InputNodeInfo *>(event->data);

    uint_fast16_t index = remote_info.index;
    assert(index < conn_.size() && conn_.at(index) == nullptr);

    uint8_t *data_ptr = get_data_ptr(index);
    fles::TimesliceComponentDescriptor *desc_ptr = get_desc_ptr(index);

    std::unique_ptr<ComputeNodeConnection> conn(new ComputeNodeConnection(
                                                                          eq_, index, compute_index_, remote_info, data_ptr, data_buffer_size_exp_,
        desc_ptr, desc_buffer_size_exp_));
    conn_.at(index) = std::move(conn);

    conn_.at(index)->on_connect_request(event, pd_, cq_);
}

/// Completion notification event dispatcher. Called by the event loop.
void ComputeBuffer::on_completion(uint64_t wr_id)
{
    size_t in = wr_id >> 8;
    assert(in < conn_.size());
    switch (wr_id & 0xFF) {

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
            assert(work_items_mq_->get_num_msg() == 0);
            assert(completions_mq_->get_num_msg() == 0);
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
                [](const std::unique_ptr<ComputeNodeConnection> &v1,
                   const std::unique_ptr<ComputeNodeConnection> &v2) {
                    return v1->cn_wp().desc < v2->cn_wp().desc;
                });

            uint64_t new_completely_written = (*new_red_lantern)->cn_wp().desc;
            red_lantern_ = std::distance(std::begin(conn_), new_red_lantern);

            for (uint64_t tpos = completely_written_;
                 tpos < new_completely_written; ++tpos) {
                if (processor_instances_ != 0) {
                    uint64_t ts_index = UINT64_MAX;
                    if (conn_.size() > 0) {
                        ts_index = get_desc(0, tpos).ts_num;
                    }
                    fles::TimesliceWorkItem wi = { { ts_index, tpos,
                                                     timeslice_size_,
                                                     static_cast<uint32_t>(
                                                         conn_.size()) },
                                                   data_buffer_size_exp_,
                                                   desc_buffer_size_exp_ };
                    work_items_mq_->send(&wi, sizeof(wi), 0);
                } else {
                    fles::TimesliceCompletion c = { tpos };
                    completions_mq_->send(&c, sizeof(c), 0);
                }
            }

            completely_written_ = new_completely_written;
        }
    } break;

    default: {
        throw LibfabricException("wc for unknown wr_id");
    }
    }
}

void ComputeBuffer::poll_ts_completion()
{
    fles::TimesliceCompletion c;
    std::size_t recvd_size;
    unsigned int priority;
    if (!completions_mq_->try_receive(&c, sizeof(c), recvd_size, priority))
        return;
    if (recvd_size == 0)
        return;
    assert(recvd_size == sizeof(c));
    if (c.ts_pos == acked_) {
        do
            ++acked_;
        while (ack_.at(acked_) > c.ts_pos);
        for (auto &connection : conn_)
            connection->inc_ack_pointers(acked_);
    } else
        ack_.at(c.ts_pos) = c.ts_pos;
}
