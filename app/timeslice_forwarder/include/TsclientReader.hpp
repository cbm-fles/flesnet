#pragma once

#include "Timeslice.hpp"
#include <cstdint>
#include "TimesliceReceiver.hpp"

#include <df/Connectors/ConnectorInterface.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/Utils/CallbackContainer.hpp>

class TsclientReader {
private:
    CallbackContainer<void()> callbacks;
    std::shared_ptr<BufferMap> buffer_map_ = nullptr;
    std::future<int> ts_reading_thread_;
    char* base_mem_addres_ = nullptr;
    std::unique_ptr<fles::Receiver<fles::Timeslice,fles::TimesliceView>> source_ = nullptr;
    uint64_t buffer_size_ = 0;
    char* buffer = nullptr;
    std::shared_ptr<ConnectorInterface> node_connector_ = nullptr;

    std::unique_ptr<fles::Timeslice> last_timeslice_ = nullptr;
    std::atomic_bool stop_ = false;
    uint64_t num_components_ = 0;
public:
    TsclientReader(std::string shm_uri);

    char* get_buffer();
    uint64_t get_buffer_size() const;
    void set_buffer_map(std::shared_ptr<BufferMap> buffer_map);

    void start_timeslice_reading();
    void on_new_timeslice(std::function<void()> cb);
    void clear_last_timeslice();

    void set_node_connector(std::shared_ptr<ConnectorInterface> node_connector);
};