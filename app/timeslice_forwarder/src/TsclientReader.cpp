#include <TsclientReader.hpp>
#include <chrono>
#include <cstdint>
#include <thread>
#include "System.hpp"
#include "Utility.hpp"
#include "df/WorkerThread.hpp"

using namespace std;
using namespace std::chrono;

TsclientReader::TsclientReader(std::string shm_uri) {
    WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll, 0,
        "AutoSource at PID " +
            to_string(fles::system::current_pid())};
    UriComponents uri{shm_uri};
    const auto shm_identifier = uri.path;
    source_ = make_unique<fles::Receiver<fles::Timeslice,fles::TimesliceView>>(shm_identifier, param);
    new_timeslice_callbacks_.set_worker(make_shared<WorkerThread>());

    // We have to read out one timeslice so the fles::Receiver class initializes the SHM and we can get necessary SHM pointer
    unique_ptr<fles::Timeslice> timeslice = source_->get();
    num_components_ = timeslice->num_components();
    buffer_size_ = source_->managed_shm_->get_size();
    buffer_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
    timeslice.reset();
}

uint64_t TsclientReader::get_buffer_size() const {
    return buffer_size_;
}

char* TsclientReader::get_buffer() {
    return buffer_;
}

void TsclientReader::clear_last_timeslice() {
    last_timeslice_ = nullptr;
    timeslice_available = false;
    cv.notify_all();
    stop_clock_ = high_resolution_clock::now();
    L_(info) << "TS reader - last_timeslice_ resetted after: " <<  duration_cast<milliseconds>(stop_clock_-start_clock_).count();
}

void TsclientReader::on_new_timeslice(std::function<void()> cb) {
    new_timeslice_callbacks_.add(cb);
}

void TsclientReader::set_buffer_map(std::shared_ptr<BufferMap> buffer_map) {
    buffer_map_ = buffer_map;
}

void TsclientReader::set_node_connector(std::shared_ptr<ConnectorInterface> node_connector) {
    node_connector_ = node_connector;
}

void TsclientReader::start_timeslice_reading() {
    ts_reading_thread_ = async([this] {

        // Buffer map needs to be set before we can start the reading of timeslices
        constexpr int sleep_timeout = 200;
        while(!buffer_map_) {
            this_thread::sleep_for(chrono::milliseconds(sleep_timeout));
        }

        unique_ptr<fles::TimesliceView> timeslice = nullptr;
        auto addresses = shared_ptr<uint64_t>(new uint64_t[num_components_ * 2], default_delete<uint64_t[]>());
        auto sizes = shared_ptr<uint64_t>(new uint64_t[num_components_ * 2], default_delete<uint64_t[]>());
        auto tags = shared_ptr<uint32_t>(new uint32_t[num_components_ * 2], default_delete<uint32_t[]>());
        time_point<high_resolution_clock> start;
        time_point<high_resolution_clock> stop;
        while (!stop_)  {
            // while (last_timeslice_ != nullptr) {};
            start = high_resolution_clock::now();
            std::unique_lock lk(m);
            cv.wait(lk, [this]{ return !timeslice_available; });

            timeslice = source_->get();
            stop = high_resolution_clock::now();
            L_(info) << "TS reader - got ts after: " <<  duration_cast<milliseconds>(stop-start).count();

            if (!timeslice) {
                break;
            }

            L_(debug) << "TS index: " << timeslice->index();

            if (buffer_ != reinterpret_cast<char*>(source_->managed_shm_->get_address())) {
                buffer_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
                L_(fatal) << "(TimesliceReader) SHM base memory address changed";
                exit(-1);
            }

            // Proactively request lock and start preparing data in the meantime
            atomic_bool is_locked = false;
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

                addresses.get()[i] = reinterpret_cast<char*>(component_desc_ptr) - buffer_;
                addresses.get()[num_components + i] = reinterpret_cast<char*>(component_data_ptr) - buffer_;
            }

            // waiting to get the lock
            start = high_resolution_clock::now();
            while (!is_locked) {};
            stop = high_resolution_clock::now();
            L_(info) << "TS reader - got buffer map after: " <<  duration_cast<milliseconds>(stop-start).count();

            // reperesent new data in the buffer map
            const auto *const buffer_map_ret = buffer_map_->insert(
                num_components * 2,
                sizes.get(),
                addresses.get(),
                0,
                0,
                tags.get(),
                BufferMap::ListElement::IO::RX
            );
            if (buffer_map_ret == nullptr) {
                L_(fatal) << "(TimesliceReader) Buffer map full. Not handled yet - exiting";
                exit(-1);
            }
            // auto *el = buffer_map_->get_oldest_linked_list_element();
            // uint64_t combined_size;
            // buffer_map_->remove_elements(buffer_map_->get_elements_of_component(el->compontent_id, combined_size));
            // timeslice.reset();
            node_connector_->unlock_buffer_map(buffer_map_);

            last_timeslice_ = std::move(timeslice);
            start_clock_ = high_resolution_clock::now();
            timeslice_available = true;
            // // tell everyone about the new data
            new_timeslice_callbacks_.call();
        }
        return int(!stop_);
    });
}

TsclientReader::~TsclientReader() {
    stop_ = true;
    try {
        ts_reading_thread_.wait();
    } catch (...) {};
}
