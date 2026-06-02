#pragma once

#include "Timeslice.hpp"
#include <chrono>
#include <cstdint>
#include "TimesliceReceiver.hpp"

#include <df/Connectors/ConnectorInterface.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/Utils/CallbackContainer.hpp>

class TsclientReader {
private:
    CallbackContainer<void()> new_timeslice_callbacks_;
    std::shared_ptr<BufferMap> buffer_map_ = nullptr;
    std::future<int> ts_reading_thread_;
    std::unique_ptr<fles::Receiver<fles::Timeslice,fles::TimesliceView>> source_ = nullptr;
    uint64_t buffer_size_ = 0;
    char* buffer_ = nullptr; ///< Pointer to the SHM where the timeslices reside
    std::shared_ptr<ConnectorInterface> node_connector_ = nullptr;

    std::unique_ptr<fles::Timeslice> last_timeslice_ = nullptr;
    std::atomic_bool stop_ = false;
    uint64_t num_components_ = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_clock_;
    std::chrono::time_point<std::chrono::high_resolution_clock> stop_clock_;
    std::mutex m;
    std::condition_variable cv;
    std::atomic_bool timeslice_available = false;
public:
    TsclientReader(std::string shm_uri);
    ~TsclientReader();
    /**
    * @brief Gets the buffer pointer.
    */
    char* get_buffer();

    /**
    * @brief Gets the buffer size.
    */
    uint64_t get_buffer_size() const;

    /**
    * @brief Setter for the buffer map. The TsclientReader takes care of representing new timeslices in the buffer map. so new timeslices can be represented in it.
    */
    void set_buffer_map(std::shared_ptr<BufferMap> buffer_map);

    /**
    * @brief starts the process of timeslice reading
    */
    void start_timeslice_reading();

    /**
    * @brief Register a callback which gets called when a new timeslice is available.
    */
    void on_new_timeslice(std::function<void()> cb);

    /**
    * @brief Destroys the last read timeslice. Needs to be called so a new timeslice can be received.
    */
    void clear_last_timeslice();

    /**
    * @brief Setter for the Connector used to transmit data, to lock the buffer map.
    * @details If the receiver receives new TS this new data needs to be represented in the buffer map.
    * The connector provides the interface to lock the buffer map.
    */
    void set_node_connector(std::shared_ptr<ConnectorInterface> node_connector);
};