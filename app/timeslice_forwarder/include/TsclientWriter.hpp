#include "ItemDistributor.hpp"
#include "ManagedTimesliceBuffer.hpp"
#include "Timeslice.hpp"
#include "TimesliceReceiver.hpp"
#include "Tssink.hpp"
#include "Utility.hpp"
#include "MyTimeslice.hpp"
#include "FragmentedTimesliceBuffer.hpp"

#include <cstdint>
#include <memory>

#include <df/Utils/CallbackContainer.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#pragma once

class TsclientWriter : public TsSink {
private:
    CallbackContainer<void()> callbacks;
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
public:

    bool handle_timeslice_completions() {
        fles::TimesliceCompletion c{};
        bool found_completion = false;
        while (ts_buffer_->try_receive_completion(c)) {
            found_completion = true;
        }
        return found_completion;
    }

    TsclientWriter(std::string shm_uri) {
        UriComponents uri{shm_uri};
        uint32_t datasize = 27; // 128 MiB
        uint32_t descsize = 19; // 16 MiB
        uint32_t num_components = 26;
        const auto shm_identifier = uri.path;
        for (auto& [key, value] : uri.query_components) {
            if (key == "datasize") {
                datasize = std::stoul(value);
            } else if (key == "descsize") {
                descsize = std::stoul(value);
            } else if (key == "n") {
                num_components = std::stoul(value);
            } else {
                throw std::runtime_error(
                    "Query parameter not implemented for scheme " + uri.scheme +
                    ": " + key);
            }
        }

        // std::cout << "shm_identifier: " << shm_identifier << std::endl;
        producer_address_ = "inproc://" + shm_identifier;
        worker_address_ = "ipc://@" + shm_identifier;

        item_distributor_ = std::make_unique<ItemDistributor>(zmq_context_, producer_address_, worker_address_),
        ts_buffer_ = std::make_shared<FragmentedTimesliceBuffer>(zmq_context_, producer_address_, shm_identifier, datasize, descsize, num_components);
        distributor_thread_ = std::thread(std::ref(*(item_distributor_.get())));
        buffer_ = std::shared_ptr<char>(static_cast<char*>(ts_buffer_->managed_shm_->get_address()));
        buffer_size_ = ts_buffer_->managed_shm_->get_size();
    }

    uint64_t get_buffer_size() override {
        return buffer_size_;
    }

    std::shared_ptr<char> get_buffer() override  {
        return std::shared_ptr<char>(buffer_.get(), no_del(char));
    }

    void on_new_timeslice(std::function<void()> cb) {
        callbacks.add(cb);
    }

    void set_buffer_map(std::shared_ptr<BufferMap> buffer_map) {
        buffer_map_ = buffer_map;
    }

    void write_timeslice(std::vector<BufferMap::ListElement*>& elements) override {
        std::vector<fles::TimesliceComponentDescriptor*> desc_ptr;
        std::vector<uint8_t*> data_ptr;
        for (auto desc_it = elements.begin(); desc_it != elements.end(); ++desc_it) {
            auto *const descriptor_el = *desc_it;
            if (1 == (descriptor_el->tag >> (sizeof(uint16_t) * 8))) { // referencing a descriptor
                desc_ptr.push_back(reinterpret_cast<fles::TimesliceComponentDescriptor*>(buffer_.get() + descriptor_el->address));
                const auto idx = static_cast<uint16_t>(descriptor_el->tag);
                for (const auto& element : elements) {
                    if (static_cast<uint16_t>(element->tag) == idx && 2 == (element->tag >> (sizeof(uint16_t) * 8))) { // is refere
                        data_ptr.push_back(reinterpret_cast<uint8_t*>(buffer_.get() + element->address));
                        break;
                    }
                }
            } // else referencing data
        }

        auto ts = std::make_shared<MyTimeslice>();
        ts->set_timeslice_descriptor({
            desc_ptr[0]->ts_num,
            0, 100,
            static_cast<uint32_t>(desc_ptr.size())
        });
        ts->timeslice_descriptor_.ts_pos = ts_cnt_++;
        ts->set_desc(std::move(desc_ptr));
        ts->set_data(std::move(data_ptr));

        ts_buffer_->send_work_item(ts);
        while (!handle_timeslice_completions()) {}
    }

    void set_buffer(std::shared_ptr<char> buffer) override {
        buffer_ = buffer;
    }

    virtual ~TsclientWriter() {
        item_distributor_->stop();
        distributor_thread_.join();
    }
};