// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "InputChannelSender.hpp"
#include "MicrosliceDescriptor.hpp"
#include "RequestIdentifier.hpp"
#include <cassert>

InputChannelSender::InputChannelSender(
    uint64_t input_index, DataSource& data_source,
    const std::vector<std::string> compute_hostnames,
    const std::vector<std::string> compute_services, uint32_t timeslice_size,
    uint32_t overlap_size, uint32_t max_timeslice_number)
    : _input_index(input_index), _data_source(data_source),
      _compute_hostnames(compute_hostnames),
      _compute_services(compute_services), _timeslice_size(timeslice_size),
      _overlap_size(overlap_size), _max_timeslice_number(max_timeslice_number),
      _min_acked_mc(data_source.desc_buffer().size() / 4),
      _min_acked_data(data_source.data_buffer().size() / 4)
{
    size_t min_ack_buffer_size =
        _data_source.desc_buffer().size() / _timeslice_size + 1;
    _ack.alloc_with_size(min_ack_buffer_size);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    VALGRIND_MAKE_MEM_DEFINED(_data_source.data_buffer().ptr(),
                              _data_source.data_buffer().bytes());
    VALGRIND_MAKE_MEM_DEFINED(_data_source.desc_buffer().ptr(),
                              _data_source.desc_buffer().bytes());
#pragma GCC diagnostic pop
}

InputChannelSender::~InputChannelSender()
{
    if (_mr_desc) {
        ibv_dereg_mr(_mr_desc);
        _mr_desc = nullptr;
    }

    if (_mr_data) {
        ibv_dereg_mr(_mr_data);
        _mr_data = nullptr;
    }
}

void InputChannelSender::report_status()
{
    uint64_t acked_ts = _acked_mc / _timeslice_size;

    std::cerr << "input channel sender " << _input_index << ": " << _acked_mc
              << " acked mc, " << acked_ts << " acked ts" << std::endl;

    auto now = std::chrono::system_clock::now();
    _scheduler.add(std::bind(&InputChannelSender::report_status, this),
                   now + std::chrono::seconds(3));
}

void InputChannelSender::sync_buffer_positions()
{
    for (auto& c : _conn) {
        c->try_sync_buffer_positions();
    }

    auto now = std::chrono::system_clock::now();
    _scheduler.add(std::bind(&InputChannelSender::sync_buffer_positions, this),
                   now + std::chrono::milliseconds(10));
}

/// The thread main function.
void InputChannelSender::operator()()
{
    try
    {
        set_cpu(2);

        connect();
        while (_connected != _compute_hostnames.size()) {
            poll_cm_events();
        }

        _time_begin = std::chrono::high_resolution_clock::now();

        uint64_t timeslice = 0;
        sync_buffer_positions();
        report_status();
        while (timeslice < _max_timeslice_number) {
            if (try_send_timeslice(timeslice)) {
                timeslice++;
            }
            poll_completion();
            _scheduler.timer();
        }

        for (auto& c : _conn) {
            c->finalize();
        }

        out.debug() << "[i" << _input_index << "] "
                    << "SENDER loop done";

        while (!_all_done) {
            poll_completion();
            _scheduler.timer();
        }

        _time_end = std::chrono::high_resolution_clock::now();

        disconnect();
        while (_connected != 0) {
            poll_cm_events();
        }

        summary();
    }
    catch (std::exception& e)
    {
        out.error() << "exception in InputChannelSender: " << e.what();
    }
}

bool InputChannelSender::try_send_timeslice(uint64_t timeslice)
{
    // wait until a complete TS is available in the input buffer
    uint64_t mc_offset = timeslice * _timeslice_size;
    uint64_t mc_length = _timeslice_size + _overlap_size;

    // This causes a decrease in performance.
    // uint64_t min_written_mc = mc_offset + mc_length + 1;
    // if (_cached_written_mc < min_written_mc) {
    //     _cached_written_mc = _data_source.written_mc();
    //     if (_cached_written_mc < min_written_mc) {
    //         return false;
    //     }
    // }

    // check if last microslice has really been written to memory
    if (_data_source.desc_buffer().at(mc_offset + mc_length).idx >
        _previous_mc_idx) {

        uint64_t data_offset = _data_source.desc_buffer().at(mc_offset).offset;
        uint64_t data_end =
            _data_source.desc_buffer().at(mc_offset + mc_length).offset;
        assert(data_end >= data_offset);

        uint64_t data_length = data_end - data_offset;
        uint64_t total_length =
            data_length + mc_length * sizeof(fles::MicrosliceDescriptor);

        if (out.beTrace()) {
            out.trace() << "SENDER working on TS " << timeslice << ", MCs "
                        << mc_offset << ".." << (mc_offset + mc_length - 1)
                        << ", data bytes " << data_offset << ".."
                        << (data_offset + data_length - 1);
            out.trace() << get_state_string();
        }

        int cn = target_cn_index(timeslice);

        if (!_conn[cn]->write_request_available())
            return false;

        // number of bytes to skip in advance (to avoid buffer wrap)
        uint64_t skip = _conn[cn]->skip_required(total_length);
        total_length += skip;

        if (_conn[cn]->check_for_buffer_space(total_length, 1)) {

            _previous_mc_idx =
                _data_source.desc_buffer().at(mc_offset + mc_length).idx;

            post_send_data(timeslice, cn, mc_offset, mc_length, data_offset,
                           data_length, skip);

            _conn[cn]->inc_write_pointers(total_length, 1);

            return true;
        }
    }

    return false;
}

std::unique_ptr<InputChannelConnection>
InputChannelSender::create_input_node_connection(uint_fast16_t index)
{
    unsigned int max_send_wr = 8000;

    // limit pending write requests so that send queue and completion queue
    // do not overflow
    unsigned int max_pending_write_requests = std::min(
        static_cast<unsigned int>((max_send_wr - 1) / 3),
        static_cast<unsigned int>((_num_cqe - 1) / _compute_hostnames.size()));

    std::unique_ptr<InputChannelConnection> connection(
        new InputChannelConnection(_ec, index, _input_index, max_send_wr,
                                   max_pending_write_requests));
    return connection;
}

void InputChannelSender::connect()
{
    for (unsigned int i = 0; i < _compute_hostnames.size(); ++i) {
        std::unique_ptr<InputChannelConnection> connection =
            create_input_node_connection(i);
        connection->connect(_compute_hostnames[i], _compute_services[i]);
        _conn.push_back(std::move(connection));
    }
}

int InputChannelSender::target_cn_index(uint64_t timeslice)
{
    return timeslice % _conn.size();
}

void InputChannelSender::dump_mr(struct ibv_mr* mr)
{
    out.debug() << "[i" << _input_index << "] "
                << "ibv_mr dump:";
    out.debug() << " addr=" << reinterpret_cast<uint64_t>(mr->addr);
    out.debug() << " length=" << static_cast<uint64_t>(mr->length);
    out.debug() << " lkey=" << static_cast<uint64_t>(mr->lkey);
    out.debug() << " rkey=" << static_cast<uint64_t>(mr->rkey);
}

void InputChannelSender::on_addr_resolved(struct rdma_cm_id* id)
{
    IBConnectionGroup<InputChannelConnection>::on_addr_resolved(id);

    if (!_mr_data) {
        // Register memory regions.
        _mr_data = ibv_reg_mr(_pd, _data_source.data_send_buffer().ptr(),
                              _data_source.data_send_buffer().bytes(),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_data) {
            out.error() << "ibv_reg_mr failed for mr_data: " << strerror(errno);
            throw InfinibandException("registration of memory region failed");
        }

        _mr_desc = ibv_reg_mr(_pd, _data_source.desc_send_buffer().ptr(),
                              _data_source.desc_send_buffer().bytes(),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_desc) {
            out.error() << "ibv_reg_mr failed for mr_desc: " << strerror(errno);
            throw InfinibandException("registration of memory region failed");
        }

        if (out.beDebug()) {
            dump_mr(_mr_desc);
            dump_mr(_mr_data);
        }
    }
}

void InputChannelSender::on_rejected(struct rdma_cm_event* event)
{
    InputChannelConnection* conn =
        static_cast<InputChannelConnection*>(event->id->context);

    conn->on_rejected(event);
    uint_fast16_t i = conn->index();
    _conn.at(i) = nullptr;

    // immediately initiate retry
    std::unique_ptr<InputChannelConnection> connection =
        create_input_node_connection(i);
    connection->connect(_compute_hostnames[i], _compute_services[i]);
    _conn.at(i) = std::move(connection);
}

std::string InputChannelSender::get_state_string()
{
    std::ostringstream s;

    s << "/--- desc buf ---" << std::endl;
    s << "|";
    for (unsigned int i = 0; i < _data_source.desc_buffer().size(); ++i)
        s << " (" << i << ")" << _data_source.desc_buffer().at(i).offset;
    s << std::endl;
    s << "| _acked_mc = " << _acked_mc << std::endl;
    s << "/--- data buf ---" << std::endl;
    s << "|";
    for (unsigned int i = 0; i < _data_source.data_buffer().size(); ++i)
        s << " (" << i << ")" << std::hex << _data_source.data_buffer().at(i)
          << std::dec;
    s << std::endl;
    s << "| _acked_data = " << _acked_data << std::endl;
    s << "\\---------";

    return s.str();
}

void InputChannelSender::post_send_data(uint64_t timeslice, int cn,
                                        uint64_t mc_offset, uint64_t mc_length,
                                        uint64_t data_offset,
                                        uint64_t data_length, uint64_t skip)
{
    int num_sge = 0;
    struct ibv_sge sge[4];
    // descriptors
    if ((mc_offset & _data_source.desc_send_buffer().size_mask()) <=
        ((mc_offset + mc_length - 1) &
         _data_source.desc_send_buffer().size_mask())) {
        // one chunk
        sge[num_sge].addr = reinterpret_cast<uintptr_t>(
            &_data_source.desc_send_buffer().at(mc_offset));
        sge[num_sge].length = sizeof(fles::MicrosliceDescriptor) * mc_length;
        sge[num_sge++].lkey = _mr_desc->lkey;
    } else {
        // two chunks
        sge[num_sge].addr = reinterpret_cast<uintptr_t>(
            &_data_source.desc_send_buffer().at(mc_offset));
        sge[num_sge].length =
            sizeof(fles::MicrosliceDescriptor) *
            (_data_source.desc_send_buffer().size() -
             (mc_offset & _data_source.desc_send_buffer().size_mask()));
        sge[num_sge++].lkey = _mr_desc->lkey;
        sge[num_sge].addr =
            reinterpret_cast<uintptr_t>(_data_source.desc_send_buffer().ptr());
        sge[num_sge].length =
            sizeof(fles::MicrosliceDescriptor) *
            (mc_length - _data_source.desc_send_buffer().size() +
             (mc_offset & _data_source.desc_send_buffer().size_mask()));
        sge[num_sge++].lkey = _mr_desc->lkey;
    }
    int num_desc_sge = num_sge;
    // data
    if (data_length == 0) {
        // zero chunks
    } else if ((data_offset & _data_source.data_send_buffer().size_mask()) <=
               ((data_offset + data_length - 1) &
                _data_source.data_send_buffer().size_mask())) {
        // one chunk
        sge[num_sge].addr = reinterpret_cast<uintptr_t>(
            &_data_source.data_send_buffer().at(data_offset));
        sge[num_sge].length = data_length;
        sge[num_sge++].lkey = _mr_data->lkey;
    } else {
        // two chunks
        sge[num_sge].addr = reinterpret_cast<uintptr_t>(
            &_data_source.data_send_buffer().at(data_offset));
        sge[num_sge].length =
            _data_source.data_send_buffer().size() -
            (data_offset & _data_source.data_send_buffer().size_mask());
        sge[num_sge++].lkey = _mr_data->lkey;
        sge[num_sge].addr =
            reinterpret_cast<uintptr_t>(_data_source.data_send_buffer().ptr());
        sge[num_sge].length =
            data_length - _data_source.data_send_buffer().size() +
            (data_offset & _data_source.data_send_buffer().size_mask());
        sge[num_sge++].lkey = _mr_data->lkey;
    }
    // copy between buffers
    for (int i = 0; i < num_sge; ++i) {
        if (i < num_desc_sge) {
            _data_source.copy_to_desc_send_buffer(
                reinterpret_cast<fles::MicrosliceDescriptor*>(sge[i].addr) -
                    _data_source.desc_send_buffer().ptr(),
                sge[i].length / sizeof(fles::MicrosliceDescriptor));
        } else {
            _data_source.copy_to_data_send_buffer(
                reinterpret_cast<uint8_t*>(sge[i].addr) -
                    _data_source.data_send_buffer().ptr(),
                sge[i].length);
        }
    }

    _conn[cn]->send_data(sge, num_sge, timeslice, mc_length, data_length, skip);
}

void InputChannelSender::on_completion(const struct ibv_wc& wc)
{
    switch (wc.wr_id & 0xFF) {
    case ID_WRITE_DESC: {
        uint64_t ts = wc.wr_id >> 24;

        int cn = (wc.wr_id >> 8) & 0xFFFF;
        _conn[cn]->on_complete_write();

        uint64_t acked_ts = _acked_mc / _timeslice_size;
        if (ts == acked_ts)
            do
                ++acked_ts;
            while (_ack.at(acked_ts) > ts);
        else
            _ack.at(ts) = ts;
        _acked_data =
            _data_source.desc_buffer().at(acked_ts * _timeslice_size).offset;
        _acked_mc = acked_ts * _timeslice_size;
        if (_acked_data >= _cached_acked_data + _min_acked_data ||
            _acked_mc >= _cached_acked_mc + _min_acked_mc) {
            _cached_acked_data = _acked_data;
            _cached_acked_mc = _acked_mc;
            _data_source.update_ack_pointers(_cached_acked_data,
                                             _cached_acked_mc);
        }
        if (out.beDebug())
            out.debug() << "[i" << _input_index << "] "
                        << "write timeslice " << ts
                        << " complete, now: _acked_data=" << _acked_data
                        << " _acked_mc=" << _acked_mc;
    } break;

    case ID_RECEIVE_CN_ACK: {
        int cn = wc.wr_id >> 8;
        _conn[cn]->on_complete_recv();
        if (_conn[cn]->done()) {
            ++_connections_done;
            _all_done = (_connections_done == _conn.size());
            out.debug() << "[i" << _input_index << "] "
                        << "ID_RECEIVE_CN_ACK final for id " << cn
                        << " all_done=" << _all_done;
        }
    } break;

    case ID_SEND_CN_WP: {
    } break;

    default:
        out.error() << "[i" << _input_index << "] "
                    << "wc for unknown wr_id=" << (wc.wr_id & 0xFF);
        throw InfinibandException("wc for unknown wr_id");
    }
}
