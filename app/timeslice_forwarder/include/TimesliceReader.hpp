#include "Timeslice.hpp"
#include <cstdint>
#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <stdatomic.h>
#include <thread>
#include "System.hpp"
#include <df/Connectors/ConnectorInterface.hpp>
#include "TimesliceReceiver.hpp"
#include "df/Utils/CallbackContainer.hpp"

class TimesliceReader {
private:
    CallbackContainer<void()> callbacks;
    std::shared_ptr<BufferMap> buffer_map_ = nullptr;
    std::future<int> ts_reading_thread_;
    char* base_mem_addres_ = nullptr;
    std::unique_ptr<fles::Receiver<fles::Timeslice,fles::TimesliceView>> source_ = nullptr;
    uint64_t buffer_size_ = 0;
    char* buffer = nullptr;
    std::shared_ptr<ConnectorInterface> node_connector_ = nullptr;

    // std::string shm_address_ = "";
    std::unique_ptr<fles::Timeslice> last_timeslice_ = nullptr;
    std::atomic_bool stop_ = false;
public:
    TimesliceReader(std::string shm_address) {
        WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll, 0,
            "AutoSource at PID " +
                std::to_string(fles::system::current_pid())};
        source_ = std::make_unique<fles::Receiver<fles::Timeslice,fles::TimesliceView>>(shm_address, param);
                // source_ = std::make_unique<fles::Receiver<fles::Timeslice,fles::TimesliceView>>(shm_address, param);

        std::unique_ptr<fles::Timeslice> timeslice = source_->get();
        buffer_size_ = source_->managed_shm_->get_size();
        buffer = reinterpret_cast<char*>(source_->managed_shm_->get_address());
        base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
        timeslice.reset();
    }

    uint64_t get_buffer_size() const {
        return buffer_size_;
    }

    char* get_buffer() {
        return buffer;
    }

    void clear_last_timeslice() {
        last_timeslice_ = nullptr;
    };

    void on_new_timeslice(std::function<void()> cb) {
        callbacks.add(cb);
    }

    void set_buffer_map(std::shared_ptr<BufferMap> buffer_map) {
        buffer_map_ = buffer_map;
    }

    void set_node_connector(std::shared_ptr<ConnectorInterface> node_connector) {
        node_connector_ = node_connector;
    }

    void start_timeslice_reading() {
        ts_reading_thread_ = std::async([this] {
            constexpr int sleep_timeout = 200;
            std::cout << "start_timeslice_reading" << std::endl;
            while(!buffer_map_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
            }


            std::unique_ptr<fles::Timeslice> timeslice = nullptr;

            while (!stop_)  {
                timeslice = source_->get();
                if (!timeslice) {
                    std::cout << "could not get timeslice" << std::endl;
                    break;
                }
                std::cout << "got new timeslice" << std::endl;

                if (base_mem_addres_ != reinterpret_cast<char*>(source_->managed_shm_->get_address())) {
                    base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
                    std::cerr << "!!! ConnectorFromFlesnet: Base memory address changed" << std::endl;
                }

                std::atomic_bool is_locked = false;
                node_connector_->lock_buffer_map(buffer_map_,
                [&is_locked] () {
                    is_locked = true;
                },
                [] () {
                    return true;
                });
                const auto num_components = timeslice->num_components();
                auto addresses = std::shared_ptr<uint64_t>(new uint64_t[num_components * 2], std::default_delete<uint64_t[]>());
                auto sizes = std::shared_ptr<uint64_t>(new uint64_t[num_components * 2], std::default_delete<uint64_t[]>());
                auto tags = std::shared_ptr<uint32_t>(new uint32_t[num_components * 2], std::default_delete<uint32_t[]>());

                // tag layout:
                // [<is_descriptor or data> (uint16_t)] [data and descriptor have the same int here (uint16_t)]
                // in more detail:
                // ['1' is descriptor, '2' is data] [idx (set by loop variable)]
                // e.g.:
                // tag: [1][8] is a descriptor that belongs to the corrosponding data element with tag [2][8]
                for (uint64_t i = 0; i < num_components; i++) {
                    auto *component_desc_ptr = timeslice->desc_ptr_.at(i);
                    auto *component_data_ptr = timeslice->data_ptr_[i];
                    sizes.get()[i] = sizeof(fles::TimesliceComponentDescriptor);
                    sizes.get()[num_components + i] = timeslice->size_component(i);

                    tags.get()[i] = static_cast<uint32_t>(1) << (sizeof(uint16_t) * 8) | static_cast<uint16_t>(i); // descriptor has tag
                    tags.get()[num_components + i] = static_cast<uint32_t>(2) << (sizeof(uint16_t) * 8) | static_cast<uint16_t>(i);

                    addresses.get()[i] = reinterpret_cast<char*>(component_desc_ptr) - base_mem_addres_;
                    addresses.get()[num_components + i] = reinterpret_cast<char*>(component_data_ptr) - base_mem_addres_;
                }

                while (!is_locked) {};
                const auto *const buffer_map_ret = buffer_map_->insert(
                    num_components * 2,
                    sizes.get(),
                    addresses.get(),
                    tags.get(),
                    BufferMap::ListElement::IO::RX
                );

                if (buffer_map_ret == nullptr) {
                    std::cout << "buffer map full" << std::endl;
                } else {
                    std::cout << "insert successfull" << std::endl;
                }
                last_timeslice_ = std::move(timeslice);
                node_connector_->unlock_buffer_map(buffer_map_);
                callbacks.call();
            }
            std::cout << "stopped reading thread" << std::endl;
            return int(!stop_);
        });
    }
};