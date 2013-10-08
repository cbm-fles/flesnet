#pragma once
/**
 * \file ComputeBuffer.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

/// Compute buffer and input node connection container class.
/** A ComputeBuffer object represents a timeslice buffer (filled by
    the input nodes) and a group of timeslice building connections to
    input nodes. */

class ComputeBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
public:
    /// The ComputeBuffer default constructor.
    explicit ComputeBuffer(uint64_t compute_index) :
        _compute_index(compute_index),
        _ack(par->cn_desc_buffer_size_exp())
    {
        boost::interprocess::shared_memory_object::remove("flesnet_data");
        boost::interprocess::shared_memory_object::remove("flesnet_desc");

        std::unique_ptr<boost::interprocess::shared_memory_object>
            data_shm(new boost::interprocess::shared_memory_object
                     (boost::interprocess::create_only, "flesnet_data",
                      boost::interprocess::read_write));
        _data_shm = std::move(data_shm);

        std::unique_ptr<boost::interprocess::shared_memory_object>
            desc_shm(new boost::interprocess::shared_memory_object
                     (boost::interprocess::create_only, "flesnet_desc",
                      boost::interprocess::read_write));
        _desc_shm = std::move(desc_shm);

        std::size_t data_size = (1 << par->cn_data_buffer_size_exp()) * par->input_nodes().size();
        _data_shm->truncate(data_size);

        std::size_t desc_size = (1 << par->cn_desc_buffer_size_exp()) * par->input_nodes().size()
            * sizeof(TimesliceComponentDescriptor);
        _desc_shm->truncate(desc_size);

        std::unique_ptr<boost::interprocess::mapped_region>
            data_region(new boost::interprocess::mapped_region
                        (*_data_shm, boost::interprocess::read_write));
        _data_region = std::move(data_region);

        std::unique_ptr<boost::interprocess::mapped_region>
            desc_region(new boost::interprocess::mapped_region
                        (*_desc_shm, boost::interprocess::read_write));
        _desc_region = std::move(desc_region);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        VALGRIND_MAKE_MEM_DEFINED(_data_region->get_address(), _data_region->get_size());
        VALGRIND_MAKE_MEM_DEFINED(_desc_region->get_address(), _desc_region->get_size());
#pragma GCC diagnostic pop

        boost::interprocess::message_queue::remove("flesnet_work_items");
        boost::interprocess::message_queue::remove("flesnet_completions");

        std::unique_ptr<boost::interprocess::message_queue>
            work_items_mq(new boost::interprocess::message_queue
                          (boost::interprocess::create_only,
                           "flesnet_work_items", 1000, sizeof(TimesliceWorkItem)));
        _work_items_mq = std::move(work_items_mq);

        std::unique_ptr<boost::interprocess::message_queue>
            completions_mq(new boost::interprocess::message_queue
                           (boost::interprocess::create_only,
                            "flesnet_completions", 1000, sizeof(TimesliceCompletion)));
        _completions_mq = std::move(completions_mq);
    }

    ComputeBuffer(const ComputeBuffer&) = delete;
    void operator=(const ComputeBuffer&) = delete;

    /// The ComputeBuffer destructor.
    ~ComputeBuffer() {
        boost::interprocess::shared_memory_object::remove("flesnet_data");
        boost::interprocess::shared_memory_object::remove("flesnet_desc");
        boost::interprocess::message_queue::remove("flesnet_work_items");
        boost::interprocess::message_queue::remove("flesnet_completions");
    }

    virtual void run() {
        //set_cpu(0);

        assert(!par->processor_executable().empty());
        child_pids.resize(par->processor_instances());
        for (uint_fast32_t i = 0; i < par->processor_instances(); ++i) {
            start_processor_task(i);
        }
        std::thread ts_compl(&ComputeBuffer::handle_ts_completion, this);

        accept(par->base_port() + par->compute_indexes().at(0), par->input_nodes().size());
        handle_cm_events(par->input_nodes().size());
        std::thread t1(&ComputeBuffer::handle_cm_events, this, 0);
        completion_handler();
        for (uint_fast32_t i = 0; i < par->processor_instances(); ++i) {
            pid_t pid = child_pids.at(i);
            child_pids.at(i) = 0;
            kill(pid, SIGTERM);
        }
        ts_compl.join();
        t1.join();

        summary();
    }

    static void start_processor_task(int i) {
        child_pids.at(i) = fork();
        if (!child_pids.at(i)) {
            std::stringstream s;
            s << i;
            execl(par->processor_executable().c_str(), s.str().c_str(), static_cast<char*>(0));
            out.fatal() << "Could not start processor task '" << par->processor_executable()
                        << " " << s.str() << "': " << strerror(errno);
            exit(1);
        }
    }

    uint8_t* get_data_ptr(uint_fast16_t index) {
        return static_cast<uint8_t*>(_data_region->get_address())
            + index * (1 << par->cn_data_buffer_size_exp());
    }

    TimesliceComponentDescriptor* get_desc_ptr(uint_fast16_t index) {
        return reinterpret_cast<TimesliceComponentDescriptor*>(_desc_region->get_address())
            + index * (1 << par->cn_desc_buffer_size_exp());
    }

    uint8_t& get_data(uint_fast16_t index, uint64_t offset) {
        offset &= (1 << par->cn_data_buffer_size_exp()) - 1;
        return get_data_ptr(index)[offset];
    }

    TimesliceComponentDescriptor& get_desc(uint_fast16_t index, uint64_t offset) {
        offset &= (1 << par->cn_desc_buffer_size_exp()) - 1;
        return get_desc_ptr(index)[offset];
    }

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct rdma_cm_event* event) {
        if (!_pd)
            init_context(event->id->verbs);

        assert(event->param.conn.private_data_len >= sizeof(InputNodeInfo));
        InputNodeInfo remote_info =
            *reinterpret_cast<const InputNodeInfo*>(event->param.conn.private_data);

        uint_fast16_t index = remote_info.index;
        assert(index < _conn.size() && _conn.at(index) == nullptr);

        uint8_t* data_ptr = get_data_ptr(index);
        std::size_t data_bytes = 1 << par->cn_data_buffer_size_exp();

        TimesliceComponentDescriptor* desc_ptr = get_desc_ptr(index);
        std::size_t desc_bytes = (1 << par->cn_desc_buffer_size_exp())
            * sizeof(TimesliceComponentDescriptor);

        std::unique_ptr<ComputeNodeConnection> conn
            (new ComputeNodeConnection(_ec, index, _compute_index, event->id, remote_info,
                                       data_ptr, data_bytes, desc_ptr, desc_bytes));
        _conn.at(index) = std::move(conn);

        _conn.at(index)->on_connect_request(event, _pd, _cq);
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) {
        size_t in = wc.wr_id >> 8;
        assert(in < _conn.size());
        switch (wc.wr_id & 0xFF) {

        case ID_SEND_CN_ACK:
            if (out.beDebug()) {
                out.debug() << "[c" << _compute_index << "] "
                            << "[" << in << "] " << "COMPLETE SEND _send_cp_ack";
            }
            _conn[in]->on_complete_send();
            break;

        case ID_SEND_FINALIZE: {
            if (par->processor_executable().empty()) {
                assert(_work_items.empty());
                assert(_completions.empty());
            } else {
                assert(_work_items_mq->get_num_msg() == 0);
                assert(_completions_mq->get_num_msg() == 0);
            }
            _conn[in]->on_complete_send();
            _conn[in]->on_complete_send_finalize();
            ++_connections_done;
            _all_done = (_connections_done == _conn.size());
            out.debug() << "[c" << _compute_index << "] "
                        << "SEND FINALIZE complete for id " << in << " all_done=" << _all_done;
            if (_all_done) {
                if (par->processor_executable().empty()) {
                    _work_items.stop();
                    _completions.stop();
                } else {
                    _completions_mq->send(nullptr, 0, 0);
                }
            }
        }
            break;

        case ID_RECEIVE_CN_WP: {
            _conn[in]->on_complete_recv();
            if (_connected == _conn.size() && in == _red_lantern) {
                auto new_red_lantern =
                    std::min_element(std::begin(_conn), std::end(_conn),
                                     [] (const std::unique_ptr<ComputeNodeConnection>& v1,
                                         const std::unique_ptr<ComputeNodeConnection>& v2)
                                     { return v1->cn_wp().desc < v2->cn_wp().desc; } );

                uint64_t new_completely_written = (*new_red_lantern)->cn_wp().desc;
                _red_lantern = std::distance(std::begin(_conn), new_red_lantern);

                for (uint64_t tpos = _completely_written; tpos < new_completely_written; ++tpos) {
                    TimesliceWorkItem wi = {tpos, par->timeslice_size(), par->overlap_size(),
                                            static_cast<uint32_t>(_conn.size()),
                                            par->cn_data_buffer_size_exp(),
                                            par->cn_desc_buffer_size_exp()};
                    if (par->processor_executable().empty()) {
                        _work_items.push(wi);
                    } else {
                        _work_items_mq->send(&wi, sizeof(wi), 0);
                    }
                }

                _completely_written = new_completely_written;
            }
        }
            break;

        default:
            throw InfinibandException("wc for unknown wr_id");
        }
    }

    virtual void handle_ts_completion() {
        //set_cpu(2);

        try {
            while (true) {
                TimesliceCompletion c;
                if (par->processor_executable().empty()) {
                    _completions.wait_and_pop(c);
                } else {
                    std::size_t recvd_size;
                    unsigned int priority;
                    _completions_mq->receive(&c, sizeof(c), recvd_size, priority);
                    if (recvd_size == 0) return;
                    assert(recvd_size == sizeof(c));
                }
                if (c.ts_pos == _acked) {
                    do
                        ++_acked;
                    while (_ack.at(_acked) > c.ts_pos);
                    for (auto& connection : _conn)
                        connection->inc_ack_pointers(_acked);
                } else
                    _ack.at(c.ts_pos) = c.ts_pos;
            }
        }
        catch (concurrent_queue<TimesliceCompletion>::Stopped) {
            out.trace() << "[c" << _compute_index << "] " << "handle_ts_completion thread done";
        }
    }

private:
    uint64_t _compute_index;

    size_t _red_lantern = 0;
    uint64_t _completely_written = 0;
    uint64_t _acked = 0;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> _ack;

    std::unique_ptr<boost::interprocess::shared_memory_object> _data_shm;
    std::unique_ptr<boost::interprocess::shared_memory_object> _desc_shm;

    std::unique_ptr<boost::interprocess::mapped_region> _data_region;
    std::unique_ptr<boost::interprocess::mapped_region> _desc_region;

    concurrent_queue<TimesliceWorkItem> _work_items;
    concurrent_queue<TimesliceCompletion> _completions;

    std::unique_ptr<boost::interprocess::message_queue> _work_items_mq;
    std::unique_ptr<boost::interprocess::message_queue> _completions_mq;
};
