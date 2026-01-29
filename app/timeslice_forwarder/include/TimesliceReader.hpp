#include "Timeslice.hpp"
#include <cstdint>
#include <thread>
#include "System.hpp"
#include "TimesliceReceiver.hpp"
#include "Utility.hpp"

#include <df/Connectors/ConnectorInterface.hpp>
#include <df/WorkerThread.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/Utils/CallbackContainer.hpp>

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

    std::unique_ptr<fles::Timeslice> last_timeslice_ = nullptr;
    std::atomic_bool stop_ = false;
    uint64_t num_components_ = 0;
public:
    TimesliceReader(std::string shm_uri) {
        WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll, 0,
            "AutoSource at PID " +
                std::to_string(fles::system::current_pid())};
        UriComponents uri{shm_uri};
        const auto shm_identifier = uri.path;

        /** Currently no parameters are available **/
        // for (auto& [key, value] : uri.query_components) {
        //     if (key == "n") {
        //         num_components_ = std::stoul(value);
        //     } else {
        //         throw std::runtime_error(
        //             "Query parameter not implemented for scheme " + uri.scheme +
        //             ": " + key);
        //     }
        // }

        source_ = std::make_unique<fles::Receiver<fles::Timeslice,fles::TimesliceView>>(shm_uri, param);

        // we have to read out one timeslice so the fles::Receiver class initializes the SHM and we can get necessary SHM pointer
        std::unique_ptr<fles::Timeslice> timeslice = source_->get();
        num_components_ = timeslice->num_components();
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

            // Buffer map needs to be set before we can start the reading of timeslices
            constexpr int sleep_timeout = 200;
            while(!buffer_map_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
            }

            std::unique_ptr<fles::Timeslice> timeslice = nullptr;
            auto addresses = std::shared_ptr<uint64_t>(new uint64_t[num_components_ * 2], std::default_delete<uint64_t[]>());
            auto sizes = std::shared_ptr<uint64_t>(new uint64_t[num_components_ * 2], std::default_delete<uint64_t[]>());
            auto tags = std::shared_ptr<uint32_t>(new uint32_t[num_components_ * 2], std::default_delete<uint32_t[]>());

            while (!stop_)  {
                timeslice = source_->get();
                if (!timeslice) {
                    break;
                }

                if (base_mem_addres_ != reinterpret_cast<char*>(source_->managed_shm_->get_address())) {
                    base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
                    std::cerr << "(TimesliceReader) SHM base memory address changed" << std::endl;
                }

                // Proactively request lock and start preparing data in the meantime
                std::atomic_bool is_locked = false;
                node_connector_->lock_buffer_map(buffer_map_,
                    [&is_locked] () {
                        is_locked = true;
                    },
                    [] () {
                        return true;
                    }
                );
                const auto num_components = timeslice->num_components();

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

                // waiting to get the lock
                while (!is_locked) {};

                // reperesent new data in the buffer map
                const auto *const buffer_map_ret = buffer_map_->insert(
                    num_components * 2,
                    sizes.get(),
                    addresses.get(),
                    tags.get(),
                    BufferMap::ListElement::IO::RX
                );
                if (buffer_map_ret == nullptr) {
                    std::cerr << "(TimesliceReader) Buffer map full. Not handled yet - exiting" << std::endl;
                    exit(-1);
                }

                last_timeslice_ = std::move(timeslice);
                node_connector_->unlock_buffer_map(buffer_map_);

                // tell everyone about the new data
                callbacks.call();
            }
            return int(!stop_);
        });
    }
};