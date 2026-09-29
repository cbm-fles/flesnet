#pragma once

#include "TimesliceShmBuffer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <zmq.hpp>

#include <df/Utils/CallbackContainer.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>

class TsclientWriter {
private:
    std::shared_ptr<BufferMap> buffer_map_ = nullptr;
    uint64_t buffer_size_ = 0;
    std::byte* buffer_ = nullptr; ///< Block in the SHM managed by the buffer map

    zmq::context_t zmq_context_{1};
    std::unique_ptr<fles::TimesliceShmBuffer> ts_buffer_ = nullptr;
    CallbackContainer<void(uint64_t ts_finished_cnt)> handled_timeslice_callbacks_;
    std::future<void> ts_completions_thread_;

    std::mutex mtx_;
    std::queue<uint64_t> component_ids_done_;

    std::atomic_uint64_t ts_input_output_cnt_diff_;
public:
    TsclientWriter(std::string output_uri);
    virtual ~TsclientWriter() = default;
    bool on_timeslices_handled(std::function<void(uint64_t)> cb);
    std::shared_ptr<char> get_buffer();
    uint64_t get_buffer_size();
    void set_buffer_map(std::shared_ptr<BufferMap> buffer_map);
    void write_timeslice(std::vector<BufferMap::ListElement*>& elements);
    bool pop_finished_component_id(uint64_t& component_id);
    uint64_t get_finished_component_id_cnt();
};
