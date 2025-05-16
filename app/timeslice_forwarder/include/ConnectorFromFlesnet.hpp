#pragma once

#include "StorableTimeslice.hpp"
#include "TimesliceAutoSource.hpp"
#include <df/Connectors/ConnectorInterface.hpp>
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
                // timeslice->
                // std::shared_ptr<const fles::StorableTimeslice> ts = std::make_shared<const fles::StorableTimeslice>(*timeslice);
                // timeslice.reset();
                // archive & *ts.get();y
                // uint64_t size = sstream.str().length() + 1;
                // std::shared_ptr<char> data = std::shared_ptr<char>(strdup(sstream.str().c_str()));
                // call_unmanaged_recv_cbs(listen_address_, data, size);
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