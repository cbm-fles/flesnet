#pragma once

#include "ManagedRingBuffer.hpp"
#include "ManagedTimesliceBuffer.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"
#include "TimesliceAutoSource.hpp"
#include <cstdint>
#include <df/Connectors/ConnectorInterface.hpp>
#include "TimesliceComponentDescriptor.hpp"
#include "TimesliceDescriptor.hpp"
#include "log.hpp"
#include <boost/archive/text_oarchive.hpp>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <iostream>

#include <TimesliceSource.hpp>

class ConnectorFromFlesnet : public ConnectorInterface {
private:
    std::string listen_address_;
    std::unique_ptr<fles::TimesliceSource> source_ = nullptr;
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

            while (!stop_listening_)  {
                std::ostringstream sstream;
                boost::archive::text_oarchive archive(sstream);
                // std::this_thread::sleep_for(std::chrono::milliseconds(10));
                std::unique_ptr<fles::Timeslice> timeslice = source_->get();
                if (!timeslice) {
                    break;
                }

                for (uint64_t i = 0; i < timeslice->num_components(); i++) {
                    auto component_data_ptr = timeslice->data_ptr_[i];
                    auto component_data_size = timeslice->size_component(i);

                    auto component_desc_ptr = timeslice->desc_ptr_.at(1);
                    auto component_desc_size = sizeof(fles::TimesliceComponentDescriptor);

                    //! @todo
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
                }

                //! @todo when timslice was transmitted call reset on pointer and remove from this vector
                timeslices.push_back(std::move(timeslice));
            }

            return int(!stop_listening_);
        });
    }

    ConnectorFromFlesnet() {
        start_timeslice_fetching();
    }

    ~ConnectorFromFlesnet() {
        stop_listening_ = true;
        // // future<> objects throws an error when we wait on it, without starting it
        try {
            ts_fetching_thread_.wait();
        } catch(...){};
    }

    /**
     * @brief Listen for clients on the given interface address
     * @param address
     * @return 0 on success, != 0 on failure
     */
     int listen_for_clients(std::string address, std::shared_ptr<char> /*custom_data = 0*/, uint64_t /*custom_data_size = 0*/) override {;
        listen_address_ = address;
        source_ = std::make_unique<fles::TimesliceAutoSource>(listen_address_);
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
        source_ = std::make_unique<fles::TimesliceAutoSource>(address);
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