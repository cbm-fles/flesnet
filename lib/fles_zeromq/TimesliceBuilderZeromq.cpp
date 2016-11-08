// Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceBuilderZeromq.hpp"
#include "MicrosliceDescriptor.hpp"
#include "TimesliceCompletion.hpp"
#include "TimesliceWorkItem.hpp"
#include "log.hpp"
#include <chrono>
#include <thread>

TimesliceBuilderZeromq::TimesliceBuilderZeromq(
    uint64_t compute_index, TimesliceBuffer& timeslice_buffer,
    const std::vector<std::string> input_server_addresses,
    uint32_t timeslice_size)
    : compute_index_(compute_index), timeslice_buffer_(timeslice_buffer),
      input_server_addresses_(input_server_addresses),
      timeslice_size_(timeslice_size), ts_index_(compute_index_),
      ack_(timeslice_buffer_.get_desc_size_exp())
{
    zmq_context_ = zmq_ctx_new();

    for (size_t i = 0; i < input_server_addresses_.size(); ++i) {
        auto input_server_address = input_server_addresses_.at(i);

        std::unique_ptr<Connection> c(new Connection{timeslice_buffer_, i});

        c->socket = zmq_socket(zmq_context_, ZMQ_REQ);
        assert(c->socket);
        int rc = zmq_connect(c->socket, input_server_address.c_str());
        assert(rc == 0);

        connections_.push_back(std::move(c));
    }
}

TimesliceBuilderZeromq::~TimesliceBuilderZeromq() {}

// TODO: make endable
// TODO: add signal handling
// TODO: consider max_timeslice

void TimesliceBuilderZeromq::operator()()
{
    assert(connections_.size() > 0);

    while (true) {
        for (auto& c : connections_) {

            do {
                // send request for timeslice data
                zmq_send(c->socket, &ts_index_, sizeof(ts_index_), 0);

                // receive desc answer (part 1), do not release
                int rc = zmq_msg_init(&c->desc_msg);
                assert(rc == 0);
                rc = zmq_msg_recv(&c->desc_msg, c->socket, 0);
                assert(rc != -1);
                if (zmq_msg_size(&c->desc_msg) == 0) {
                    zmq_msg_close(&c->desc_msg);
                }
            } while (zmq_msg_size(&c->desc_msg) == 0);

            // receive data answer (part 2), do not release
            assert(zmq_msg_more(&c->data_msg));
            int rc = zmq_msg_init(&c->data_msg);
            assert(rc == 0);
            rc = zmq_msg_recv(&c->data_msg, c->socket, 0);
            assert(rc != -1);

            uint64_t size_required =
                zmq_msg_size(&c->desc_msg) + zmq_msg_size(&c->data_msg);

            while (c->data.size_available() < size_required ||
                   c->desc.size_available() < 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                handle_timeslice_completions();
            }

            // generate timeslice component descriptor
            assert(tpos == c->desc.write_index());
            c->desc.append({ts_index_, c->data.write_index(), size_required,
                            zmq_msg_size(&c->desc_msg) /
                                sizeof(fles::MicrosliceDescriptor)});

            // copy into shared memory and release messages
            c->data.append(static_cast<uint8_t*>(zmq_msg_data(&c->desc_msg)),
                           zmq_msg_size(&c->desc_msg));
            c->data.append(static_cast<uint8_t*>(zmq_msg_data(&c->data_msg)),
                           zmq_msg_size(&c->data_msg));
            zmq_msg_close(&c->desc_msg);
            zmq_msg_close(&c->data_msg);
        }
        handle_timeslice_completions();

        timeslice_buffer_.send_work_item(
            {{ts_index_, tpos, timeslice_size_,
              static_cast<uint32_t>(connections_.size())},
             timeslice_buffer_.get_data_size_exp(),
             timeslice_buffer_.get_desc_size_exp()});
        ++tpos;
        // next timeslice: round robin
        ts_index_ += compute_index_;
    }
}

void TimesliceBuilderZeromq::handle_timeslice_completions()
{
    fles::TimesliceCompletion c;
    while (timeslice_buffer_.try_receive_completion(c)) {
        if (c.ts_pos == acked_) {
            do
                ++acked_;
            while (ack_.at(acked_) > c.ts_pos);
            for (auto& conn : connections_) {
                conn->desc.set_read_index(acked_);
                conn->data.set_read_index(conn->desc.at(acked_ - 1).offset +
                                          conn->desc.at(acked_ - 1).size);
            }
        } else
            ack_.at(c.ts_pos) = c.ts_pos;
    }
}
