#pragma once

#include "ItemDistributor.hpp"
#include "ManagedTimesliceBuffer.hpp"
#include "Timeslice.hpp"
#include "TimesliceReceiver.hpp"
#include "Tssink.hpp"
#include "MyTimeslice.hpp"
#include "FragmentedTimesliceBuffer.hpp"

#include <cstdint>
#include <memory>

#include <df/Utils/CallbackContainer.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>

class TsclientWriter : public TsSink {
private:
    std::shared_ptr<BufferMap> buffer_map_ = nullptr;
    std::future<int> ts_reading_thread_;
    std::unique_ptr<fles::Receiver<fles::Timeslice,fles::TimesliceView>> source_ = nullptr;
    uint64_t buffer_size_ = 0;
    std::shared_ptr<char> buffer_ = nullptr;
    std::shared_ptr<ConnectorInterface> node_connector_ = nullptr;

    // std::string shm_address_ = "";
    std::unique_ptr<fles::Timeslice> last_timeslice_ = nullptr;
    std::atomic_bool stop_ = false;
    std::unique_ptr<MyTimesliceArchive> ts_sink_ = nullptr;
    zmq::context_t zmq_context_{1};
    std::shared_ptr<ManagedTimesliceBuffer> managed_timeslice_buffer = nullptr;
    std::shared_ptr<FragmentedTimesliceBuffer> ts_buffer_ = nullptr;
    std::unique_ptr<ItemDistributor> item_distributor_ = nullptr;
    std::thread distributor_thread_;
    std::string producer_address_;
    std::string worker_address_;
    uint64_t ts_cnt_ = 0;
    bool handle_timeslice_completions();

public:
    TsclientWriter(std::string output_uri);
    virtual ~TsclientWriter() override;
    void on_new_timeslice(std::function<void()> cb);

    std::shared_ptr<char> get_buffer() override;

    uint64_t get_buffer_size() override;

    void set_buffer_map(std::shared_ptr<BufferMap> buffer_map);

    void write_timeslice(std::vector<BufferMap::ListElement*>& elements) override;
};
