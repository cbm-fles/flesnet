#include "FragmentedTimesliceBuffer.hpp"
#include "ItemDistributor.hpp"
#include "ManagedTimesliceBuffer.hpp"
#include "OutputArchive.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"
#include "TimesliceReceiver.hpp"
#include <cstdint>
#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <memory>
#include "TimesliceShmWorkItem.hpp"
#include "TimesliceWorkItem.hpp"
#include "Tssink.hpp"
#include "df/Utils/CallbackContainer.hpp"
#include "MyTimeslice.hpp"

#pragma once

class TsclientWriter : public TsSink {
private:
    CallbackContainer<void()> callbacks;
    std::shared_ptr<BufferMap> buffer_map_ = nullptr;
    std::future<int> ts_reading_thread_;
    char* base_mem_addres_ = nullptr;
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
    // std::shared_ptr<TimesliceBuffer> ts_buffer_ = nullptr;
    uint64_t acked_ = 0;
    std::unique_ptr<ItemDistributor> item_distributor_ = nullptr;
    std::thread distributor_thread_;
    std::string producer_address_;
    std::string worker_address_;

public:

bool handle_timeslice_completions() {
  fles::TimesliceCompletion c{};
  bool found_completion = false;
  while (ts_buffer_->try_receive_completion(c)) {
    std::cout << "completion received" << std::endl;
    found_completion = true;
    // if (c.ts_pos == acked_) {
    //   do {
    //     ++acked_;
    //   } while (ack_.at(acked_) > c.ts_pos);
    //   for (std::size_t i = 0; i < desc_.size(); ++i) {
    //     desc_.at(i).set_read_index(acked_);
    //     data_.at(i).set_read_index(desc_.at(i).at(acked_ - 1).offset +
    //                                desc_.at(i).at(acked_ - 1).size);
    //   }
    // } else {
    //   ack_.at(c.ts_pos) = c.ts_pos;
    // }
  }
  return found_completion;
}

    TsclientWriter(std::string shm_identifier) {
        uint32_t datasize = 27; // 128 MiB
        uint32_t descsize = 19; // 16 MiB
        // producer_address_ = "inproc://" + shm_identifier;
        // worker_address_ = "ipc://@" + shm_identifier;
        producer_address_ = "inproc://ts_in";
        worker_address_ = "ipc://@ts_in";

        item_distributor_ = std::make_unique<ItemDistributor>(zmq_context_, producer_address_, worker_address_),
        ts_buffer_ = std::make_shared<FragmentedTimesliceBuffer>(zmq_context_, producer_address_, shm_identifier, datasize, descsize, 26);
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

    // void clear_last_timeslice() override {
    //     last_timeslice_ = nullptr;
    // };

    void on_new_timeslice(std::function<void()> cb) {
        callbacks.add(cb);
    }

    void set_buffer_map(std::shared_ptr<BufferMap> buffer_map) {
        buffer_map_ = buffer_map;
    }

    void set_node_connector(std::shared_ptr<ConnectorInterface> node_connector) {
        node_connector_ = node_connector;
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


        std::cout << "create timeslice 6" << std::endl;
        std::cout << "desc_ptr.size(): " << desc_ptr.size() << std::endl;
        std::cout << "data_ptr.size(): " << data_ptr.size() << std::endl;
        std::cout << "desc_ptr[0]->ts_num: " << desc_ptr[0]->ts_num << std::endl;
        auto ts = std::make_shared<MyTimeslice>();
        std::cout << "create timeslice 7" << std::endl;

        ts->set_timeslice_descriptor({
            desc_ptr[0]->ts_num,
            0, 100,
            static_cast<uint32_t>(desc_ptr.size())
        });
        std::cout << "create timeslice 8" << std::endl;

        ts->set_desc(desc_ptr);
        ts->set_data(data_ptr);

        // ts_sink_->put(std::reinterpret_pointer_cast<fles::StorableTimeslice>(ts));
        std::cout << "ts->timeslice_descriptor_.num_core_microslices: " << ts->timeslice_descriptor_.num_core_microslices << std::endl;
        std::cout << "ts->num_microslices(0): " << ts->num_microslices(0) << std::endl;

        std::cout << "ts_index: " << ts->index() << std::endl;
        // std::cout << "before rmove elemeents" << std::endl;
        auto tsd = ts->timeslice_descriptor_;
        std::cout << "tsd.num_core_microslices: " << tsd.num_core_microslices << std::endl;
        // tsd.ts_pos = ts_pos_++;

        // ts_buffer_->send_work_item({tsd, ts_buffer_->get_data_size_exp(), ts_buffer_->get_desc_size_exp()});
        ts_buffer_->send_work_item(ts);
        while (!handle_timeslice_completions()) {}
        // std::cout << "sleeping..." << std::endl;
        // sleep(4);
        // std::cout << "sleeping done" << std::endl;

        // sleep(3);
        // managed_timeslice_buffer->put(ts);
        // buffer_map_->remove_elements(elements);
        // buffer_map_->unlock();
        std::cout << "create timeslice done" << std::endl;
        // ts_sink_ = nullptr;

    }

    void set_buffer(std::shared_ptr<char> buffer) override {
        buffer_ = buffer;
    }

    virtual ~TsclientWriter() {
        item_distributor_->stop();
      distributor_thread_.join();
    }
};