#pragma once

#include "ManagedRingBuffer.hpp"
#include "Timeslice.hpp"
#include "TimesliceAutoSource.hpp"
#include <cstdint>
#include <df/Connectors/ConnectorInterface.hpp>
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include "TimesliceReceiver.hpp"
#include "log.hpp"
#include <boost/archive/text_oarchive.hpp>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <iostream>
#include "System.hpp"
#include <memory>

#include <TimesliceSource.hpp>
using namespace std;

class ConnectorFromFlesnet : public ConnectorInterface {
private:
    std::string listen_address_;
    // std::unique_ptr<fles::TimesliceSource> source_ = nullptr;
    char* base_mem_addres_ = nullptr;

    std::unique_ptr<fles::Receiver<fles::Timeslice,fles::TimesliceView>> source_ = nullptr;
    std::future<int> ts_fetching_thread_;
    std::atomic_bool stop_listening_ = false;

    std::mutex unmanaged_recv_mtx_;
    std::vector<std::function<void (std::string address, std::shared_ptr<char> data, uint64_t size)>> unmanaged_recv_callbacks_;
    std::vector<std::unique_ptr<fles::Timeslice>> timeslices;
public:
    void start_timeslice_fetching() {
        ts_fetching_thread_ = std::async([this] {
            const int sleep_timeout = 200;

            while (!source_ && !stop_listening_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
            }
            std::unique_ptr<fles::Timeslice> timeslice = source_->get();
            timeslice.reset();
            global_buffer_size_ = source_->managed_shm_->get_size();
            global_buffer_ = std::shared_ptr<char>(reinterpret_cast<char*>(source_->managed_shm_->get_address()));
            base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
            while(!buffer_map_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
            }
            auto metadata = buffer_map_->get_list_metadata();

            while (!stop_listening_)  {
                // std::ostringstream sstream;
                // boost::archive::text_oarchive archive(sstream);

                // std::this_thread::sleep_for(std::chrono::milliseconds(10));
                timeslice = source_->get();
                if (!timeslice) {
                    cout << "could not get timeslice" << endl;
                    break;
                }

                if (base_mem_addres_ != reinterpret_cast<char*>(source_->managed_shm_->get_address())) {
                    cerr << "!!! ConnectorFromFlesnet: Base memory address changed" << endl;
                }
                // base_mem_addres_
                // cout << reinterpret_cast<uint64_t>(base_mem_addres_) << endl;
                // source_->managed_shm_->
                // cout << "got timeslice" << endl;
                auto addresses = std::shared_ptr<uint64_t>(new uint64_t[timeslice->num_components() * 2]);
                auto sizes = std::shared_ptr<uint64_t>(new uint64_t[timeslice->num_components() * 2]);

                for (uint64_t i = 0; i < timeslice->num_components(); i++) {
                    // cout << "ts 1" << endl;
                    auto *component_data_ptr = timeslice->data_ptr_[i];
                    sizes.get()[i * 2] = timeslice->size_component(i);
                    // cout << "ts 2" << endl;

                    auto *component_desc_ptr = timeslice->desc_ptr_.at(1);
                    sizes.get()[i * 2 + 1] = sizeof(fles::TimesliceComponentDescriptor);
                    // cout << "ts 3" << endl;

                    addresses.get()[i * 2] = reinterpret_cast<char*>(component_data_ptr) - base_mem_addres_;
                    addresses.get()[i * 2 + 1] = reinterpret_cast<char*>(component_desc_ptr) - base_mem_addres_;
                    // cout << "ts 4" << endl;

                    // if (component_data_ptr_diff > global_buffer_size_) {
                    //     std::cerr << "Invalid component_data_ptr position: " << std::endl;
                    //     std::cerr << "- found at: " << (component_data_ptr - reinterpret_cast<unsigned char*>(base_mem_addres_)) << std::endl;
                    //     std::cerr << "- diff: " << component_data_ptr_diff - global_buffer_size_ << std::endl;
                    // } else {
                    //     std::cout << "data_ptr ok" << std::endl;
                    // }

                    // if (component_desc_ptr_diff > global_buffer_size_) {
                    //     // std::cerr << "Invalid component_desc_ptr position" << std::endl;
                    //     // std::cerr << "- found at: " << (reinterpret_cast<unsigned char*>(component_desc_ptr) - reinterpret_cast<unsigned char*>(base_mem_addres_)) << std::endl;
                    //     // std::cerr << "- diff: " << component_desc_ptr_diff - global_buffer_size_ << std::endl;
                    // } else {
                    //     std::cout << "desc_ptr ok" << std::endl;

                    // }
                    //! @todo
                    /*
                    - Older example transfering simple name list is working again with new BUfferMap impl.
                        - Sending and evaluating buffer map on CM
                        - Streamlineing metadata exchange (NodeId initial BufferMap transmission)
                        - Issues with TCP: seg fault connection establishement
                    - Collectl
                        -
                    */

                    // Global Buffer Using SHM of flesnet:
                    // ./tsclient -i file:///scratch/htc/bzcschin/mcbm_data/2022_gold_partial/2488_node17_1_0001.tsa -o "shm://127.0.0.1/tsclientout?n=29"&
                    //  auto shm_name = "tsclientout";
                    //  auto shm_fd = shmopen(tsclientout, O_RDRW);
                    //  ftruncate(shm_fd, size_of_shm); // maybe not
                    //  auto shm_ptr = mmap(0, size_of_shm, PROT_WRITE | PROT_READ, MAP_SHARED, shm_id, 0);
                    // On Node:
                    //  - Extend BufferMap interface so I can decide the memory address (the decision about the memory pos was obsviously already made by the outside world)
                    //  - Insert multiple items at once into the buffer map which then *belong together* so that all these items are transmitted together
                    //  Input:
                    //   - Update buffer map with component_data_ptr addr and size;
                    //   - Update buffer map with component_desc_ptr addr and size;
                    //  Output:
                    //   - Extend `ManagedTimesliceBuffer` so the write head can be moved forward appropriately to an already received RDMA transmission
                    //   -
                    // On CM:
                    // - Extend `ManagedTimesliceBuffer` so the current write and read idx can be set to calculate the buffer offset for the next insertion:
                    //      Plan:
                    //      1. CM recv buffer map/current ring buffer state
                    //      2. Use extended ManagedTimesliceBuffer to "hypthatically insert" and get the adddress(es) of the insert
                    //      3. Command the entry node to transmit TS descriptors and compontent data to the calculated adresses
                    //      4. On successfull receive update the actual ManagedTimesliceBuffer with the newly received TS
                    //

                    // Start working in the poster
                }
                uint64_t expected = false;
                while (!(metadata->list_lock.compare_exchange_weak(expected, true) && expected == false));
                auto *buffer_map_ret = buffer_map_->insert(
                    timeslice->num_components() * 2,
                    sizes.get(),
                    addresses.get(),
                    BufferMap::ListElement::IO::RX
                );
                metadata->list_lock = false;

                if (buffer_map_ret == nullptr) {
                    cout << "buffer map full" << endl;
                }
                cout << "ConnectorFromFlesnet: before callback" << endl;
                call_unmanaged_recv_cbs_sync(listen_address_, nullptr, 0);
                cout << "ConnectorFromFlesnet: after callback" << endl;
                // timeslices.push_back(std::move(timeslice));

                timeslice.reset();
                //! @todo when timslice was transmitted call reset on pointer and remove from this vector
            }

            return int(!stop_listening_);
        });
    }

    ConnectorFromFlesnet() {
        start_timeslice_fetching();
    }

    ~ConnectorFromFlesnet() {
        stop_listening_ = true;
        // future<> objects throws an error when we wait on it, without starting it
        try {
            ts_fetching_thread_.wait();
        } catch(...){};
    }

    // do nothing - the global buffer and its size is set when connected to the flesnet shared memory
    int set_global_buffer(std::shared_ptr<char> /*buffer*/, uint64_t /*buffer_size*/) override {
        return 0;
    }

    /**
     * @brief Listen for clients on the given interface address
     * @param address
     * @return 0 on success, != 0 on failure
     */
     int listen_for_clients(std::string address, std::shared_ptr<char> /*custom_data = 0*/, uint64_t /*custom_data_size = 0*/) override {;
        listen_address_ = address;
        WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll, 0,
            "AutoSource at PID " +
                std::to_string(fles::system::current_pid())};
        // cout << "1" << endl;
        source_ = std::make_unique<fles::Receiver<fles::Timeslice,fles::TimesliceView>>(listen_address_, param);
        // source_ = std::make_unique<fles::TimesliceAutoSource>(address);
        // cout << "2" << endl;

        // base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
        // cout << "3" << endl;

        // source_ = std::make_unique<fles::TimesliceAutoSource>(listen_address_);
        return 0;
    }

    /**
     * @brief Get the address this connector is listening on.
     * @details The returned address does not have to be the same as the listen address
     * given in to the listen_for_clients() method
     * @note the returned address will appear as the argument of connect_to_server() on a different node
     * @param address
     * @return 0 on success, != 0 on failure
     */
    std::string get_listen_address() override {
        return listen_address_;
    };

    /**
     * @brief Connects to the server at the given address
     * @note the address argument was produced by the get_listen_address() of another node
     * @param address the address to connect to
     * @return 0 on success, != 0 on failure
     */
     int connect_to_server(std::string address, std::shared_ptr<char> /*custom_data = nullptr*/, uint64_t /*custom_data_size = 0*/) override {;
        // source_ = std::make_unique<fles::TimesliceAutoSource>(address);
        WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll, 0,
            "Data Forwarder (ConnectorFromFlesnet) at PID " +
                std::to_string(fles::system::current_pid())};

        source_ = std::make_unique<fles::Receiver<fles::Timeslice,fles::TimesliceView>>(address, param);

        return 0;
    }


    int recv(std::string /*address*/, std::shared_ptr<char> /*buff*/, uint64_t /*size*/) override {
        // throw std::runtime_error(ERR_STR("not implemented"));
        throw std::runtime_error("not implemented");
        return -1;
    }

    int send(std::string /*address*/, std::shared_ptr<char> /*buff*/, uint64_t /*size*/) override {
        throw std::runtime_error("not implemented");
        // throw std::runtime_error(ERR_STR("not implemented"));
        return -1;
    }

};