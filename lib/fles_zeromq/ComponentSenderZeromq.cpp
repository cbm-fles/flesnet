// Copyright 2012-2013, 2016 Jan de Cuveland <cmail@cuveland.de>

#include "ComponentSenderZeromq.hpp"
#include "MicrosliceDescriptor.hpp"
#include "log.hpp"
#include <algorithm>
#include <cassert>

ComponentSenderZeromq::ComponentSenderZeromq(
    uint64_t input_index, InputBufferReadInterface& data_source,
    std::string listen_address, uint32_t timeslice_size, uint32_t overlap_size,
    uint32_t max_timeslice_number, volatile sig_atomic_t* signal_status)
    : input_index_(input_index), data_source_(data_source),
      timeslice_size_(timeslice_size), overlap_size_(overlap_size),
      max_timeslice_number_(max_timeslice_number),
      signal_status_(signal_status),
      min_acked_({data_source.desc_buffer().size() / 4,
                  data_source.data_buffer().size() / 4})
{
    start_index_ = acked_ = cached_acked_ = data_source.get_read_index();

    size_t min_ack_buffer_size =
        data_source_.desc_buffer().size() / timeslice_size_ + 1;
    ack_.alloc_with_size(min_ack_buffer_size);

    zmq_context_ = zmq_ctx_new();
    socket_ = zmq_socket(zmq_context_, ZMQ_REP);
    int rc = zmq_bind(socket_, listen_address.c_str());
    assert(rc == 0);
}

ComponentSenderZeromq::~ComponentSenderZeromq()
{
    if (zmq_context_) {
        zmq_ctx_destroy(zmq_context_);
    }
}

void ComponentSenderZeromq::operator()()
{
    while (acked_ts_ < max_timeslice_number_ && *signal_status_ == 0) {
        zmq_msg_t request;
        int rc = zmq_msg_init(&request);
        assert(rc == 0);

        int len = zmq_recvmsg(socket_, &request, 0);
        assert(len != -1);

        assert(len == sizeof(uint64_t));
        uint64_t timeslice = *static_cast<uint64_t*>(zmq_msg_data(&request));
        zmq_msg_close(&request);

        try_send_timeslice(timeslice);
    }
    sync_data_source();
}

struct Acknowledgment {
    ComponentSenderZeromq* server;
    uint64_t timeslice;
    bool is_data;
};

void free_ts(void* /* data */, void* hint)
{
    assert(hint);
    auto* ack = static_cast<Acknowledgment*>(hint);
    ack->server->ack_timeslice(ack->timeslice, ack->is_data);
    delete ack;
}

bool ComponentSenderZeromq::try_send_timeslice(uint64_t ts)
{
    assert(ts >= acked_ts_);

    uint64_t desc_offset = ts * timeslice_size_ + start_index_.desc;
    uint64_t desc_length = timeslice_size_ + overlap_size_;

    // check if complete timeslice is available in the input buffer
    if (write_index_desc_ < desc_offset + desc_length) {
        write_index_desc_ = data_source_.get_write_index().desc;
        if (write_index_desc_ < desc_offset + desc_length) {
            // send empty message
            zmq_msg_t msg;
            zmq_msg_init_size(&msg, 0);
            zmq_msg_send(&msg, socket_, 0);
            return false;
        }
    }

    // part 1: descriptors
    auto desc_msg = create_message(data_source_.desc_buffer(), desc_offset,
                                   desc_length, ts, false);
    zmq_msg_send(&desc_msg, socket_, ZMQ_SNDMORE);

    // part 2: data
    uint64_t data_offset = data_source_.desc_buffer().at(desc_offset).offset;
    uint64_t data_end =
        data_source_.desc_buffer().at(desc_offset + desc_length - 1).offset +
        data_source_.desc_buffer().at(desc_offset + desc_length - 1).size;
    assert(data_end >= data_offset);
    uint64_t data_length = data_end - data_offset;

    auto data_msg = create_message(data_source_.data_buffer(), data_offset,
                                   data_length, ts, true);
    zmq_msg_send(&data_msg, socket_, 0);

    return true;
}

template <typename T_>
zmq_msg_t ComponentSenderZeromq::create_message(RingBufferView<T_>& buf,
                                                uint64_t offset,
                                                uint64_t length, uint64_t ts,
                                                bool is_data)
{
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
        auto* hint = new Acknowledgment{this, ts, is_data};
        zmq_msg_init_data(&msg, data, bytes, free_ts, hint);
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

void ComponentSenderZeromq::ack_timeslice(uint64_t ts, bool is_data)
{
    assert(ts >= acked_ts_);
    if (ts != acked_ts_) {
        // transmission has been reordered, store completion information
        if (is_data) {
            ack_.at(ts).data = ts;
        } else {
            ack_.at(ts).desc = ts;
        }
    } else {
        // completion is for earliest pending timeslice, update indices
        do {
            ++acked_ts_;
        } while (ack_.at(acked_ts_).desc > ts && ack_.at(acked_ts_).data > ts);
        acked_.desc = acked_ts_ * timeslice_size_ + start_index_.desc;
        acked_.data = data_source_.desc_buffer().at(acked_.desc - 1).offset +
                      data_source_.desc_buffer().at(acked_.desc - 1).size;
        if (acked_.data >= cached_acked_.data + min_acked_.data ||
            acked_.desc >= cached_acked_.desc + min_acked_.desc) {
            cached_acked_ = acked_;
            data_source_.set_read_index(cached_acked_);
        }
    }
}

void ComponentSenderZeromq::sync_data_source()
{
    if (acked_.data > cached_acked_.data || acked_.desc > cached_acked_.desc) {
        cached_acked_ = acked_;
        data_source_.set_read_index(cached_acked_);
    }
}
