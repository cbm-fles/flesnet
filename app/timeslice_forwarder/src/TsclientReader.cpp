#include <TsclientReader.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include "System.hpp"
#include "Timeslice.hpp"
#include "Utility.hpp"
#include "df/WorkerThread.hpp"

using namespace std;
using namespace std::chrono;

TsclientReader::TsclientReader(std::string shm_uri) {
    WorkerParameters param{
        1,
        0,
        WorkerQueuePolicy::QueueAll,
        0,
        "AutoSource at PID " +
        to_string(fles::system::current_pid())
    };
    UriComponents uri{shm_uri};
    const auto shm_identifier = uri.path;
    // The SHM is mapped writable, as needed for the RDMA memory registration
    source_ = make_unique<fles::Receiver<fles::Timeslice, fles::TimesliceView>>(shm_identifier, param, fles::ShmAccess::ReadWrite);
    new_timeslice_callbacks_.set_worker(make_shared<WorkerThread>());

    // We have to read out one timeslice to get the SHM region to register it for RDMA transmissions.
    // It is kept and sent first.
    first_timeslice_ = source_->get();
    if (!first_timeslice_) {
        throw runtime_error("(TimesliceReader) no timeslice received from " + shm_uri);
    }
    const auto region = first_timeslice_->shm_region();
    shm_uuid_ = first_timeslice_->shm_uuid();
    buffer_size_ = region.size();
    // The region is const because timeslices are read-only, but the mapping is writable (see above)
    buffer_ = const_cast<char*>(reinterpret_cast<const char*>(region.data()));
}

uint64_t TsclientReader::get_buffer_size() const {
    return buffer_size_;
}

char* TsclientReader::get_buffer() {
    return buffer_;
}

void TsclientReader::clear_last_timeslice() {
    {
        std::unique_lock lk(m);
        //last_timeslice_.reset();
        last_timeslice_ = nullptr;
        timeslice_available = false;
    }
    cv.notify_all();
    stop_clock_ = high_resolution_clock::now();
    L_(trace) << "TS reader - last_timeslice_ resetted after: " <<  duration_cast<milliseconds>(stop_clock_-start_clock_).count();
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

        unique_ptr<fles::TimesliceView> ts = nullptr;
        time_point<high_resolution_clock> start;
        time_point<high_resolution_clock> stop;
        while (!stop_)  {
            start = high_resolution_clock::now();
            std::unique_lock lk(m);
            L_(trace) << "waiting for consumption";
            cv.wait(lk, [this]{ return !timeslice_available; });
            //sleep(6);
            L_(trace) << "Getting new";

            ts = first_timeslice_ ? std::move(first_timeslice_) : source_->get();

            stop = high_resolution_clock::now();
            L_(trace) << "TS reader - got ts after: " <<  duration_cast<milliseconds>(stop-start).count();
            if (!ts) {
                L_(debug) << "ts is null";
                break;
            }

            L_(debug) << "TS index: " << ts->index();
            if (ts->shm_uuid() != shm_uuid_) {
                L_(fatal) << "(TimesliceReader) SHM segment changed";
                exit(-EXIT_FAILURE);
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
            // One element per component, in component order. The tag holds the component flags,
            // the receiver derives the other descriptor fields from the data.
            const auto desc = ts->st_descriptor();
            const auto num_components = desc.components.size();
            vector<uint64_t> sizes(num_components);
            vector<uint64_t> addresses(num_components);
            vector<uint32_t> tags(num_components);
            for (uint64_t i = 0; i < num_components; i++) {
                sizes[i] = desc.components[i].ms_data_size;
                addresses[i] = desc.components[i].ms_data_offset; // relative to the SHM region
                tags[i] = desc.components[i].flags;
            }

            // waiting to get the lock
            start = high_resolution_clock::now();
            while (!is_locked) {};
            stop = high_resolution_clock::now();
            L_(trace) << "TS reader - got buffer map after: " <<  duration_cast<milliseconds>(stop-start).count();

            // reperesent new data in the buffer map
            auto *const buffer_map_ret = buffer_map_->insert(
                num_components,
                sizes.data(),
                addresses.data(),
                0,
                0,
                tags.data(),
                BufferMap::ListElement::IO::RX
            );

            if (buffer_map_ret == nullptr) {
                L_(fatal) << "(TimesliceReader) Buffer map full. Not handled yet - exiting";
                exit(-EXIT_FAILURE);
            }

            // first element of the insertion will contain the TS index
            buffer_map_ret->user_0 = ts->index();
            node_connector_->unlock_buffer_map(buffer_map_);
            last_timeslice_ = std::move(ts);
            start_clock_ = high_resolution_clock::now();
            timeslice_available = true;
            // tell everyone about the new data
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
