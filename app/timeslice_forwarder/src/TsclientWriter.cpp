#include "TsclientWriter.hpp"
#include "Utility.hpp"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <chrono>

using namespace std;
using namespace std::chrono;

uint64_t TsclientWriter::handle_timeslice_completions() {
    fles::TimesliceCompletion c{};
    uint64_t found_completions = 0;
    while (ts_buffer_->try_receive_completion(c)) {
        found_completions++;
    }

    return found_completions;
}

bool TsclientWriter::on_timeslices_handled(std::function<void(uint64_t)> cb) {
    return handled_timeslice_callbacks_.add(cb);
}

TsclientWriter::TsclientWriter(std::string output_uri, uint32_t timeslice_size) : timeslice_size_(timeslice_size) {
    UriComponents uri{output_uri};
    uint32_t datasize = 27; // 128 MiB
    uint32_t descsize = 19; // 16 MiB
    uint32_t num_components = 26;
    const auto shm_identifier = uri.path;
    const auto sheme = uri.scheme;

    for (auto& [key, value] : uri.query_components) {
        if (key == "datasize") {
            datasize = stoul(value);
        } else if (key == "descsize") {
            descsize = stoul(value);
        } else if (key == "n") {
            num_components = stoul(value);
        } else {
            throw runtime_error(
                "Query parameter not implemented for scheme " + uri.scheme +
                ": " + key);
        }
    }

    producer_address_ = "inproc://" + shm_identifier;
    worker_address_ = "ipc://@" + shm_identifier;

    item_distributor_ = make_unique<ItemDistributor>(zmq_context_, producer_address_, worker_address_),
    ts_buffer_ = make_shared<FragmentedTimesliceBuffer>(zmq_context_, producer_address_, shm_identifier, datasize, descsize, num_components);
    distributor_thread_ = thread(ref(*(item_distributor_.get())));
    buffer_ = shared_ptr<char>(static_cast<char*>(ts_buffer_->get_shm_ptr()));

    buffer_size_ = ts_buffer_->get_shm_size();
    handled_timeslice_callbacks_.set_worker(make_shared<WorkerThread>());

    ts_completions_thread_ = std::async([this] () {
        while (true) {
            fles::TimesliceCompletion c{};
            uint64_t found_completions = 0;
            {
                L_(trace) << "ts_completions_thread_ - before mtx";
                unique_lock<mutex> l(mtx_);
                L_(trace) << "ts_completions_thread_ - try_receive_completion ...";
                while (ts_buffer_->try_receive_completion(c)) {
                    L_(debug) << "c.ts_pos: " << c.ts_pos << " - tspos_componentid_map_[c.ts_pos]: " << tspos_componentid_map_[c.ts_pos];
                    component_ids_done_.push(tspos_componentid_map_[c.ts_pos]);
                    tspos_componentid_map_.erase(c.ts_pos);
                    found_completions++;
                }
            }

            if (found_completions != 0) {
                ts_input_output_cnt_diff_ -= found_completions;
                L_(debug) << "ts_completions_thread_ - open completions: " << ts_input_output_cnt_diff_;
                L_(trace) << "ts_completions_thread_ - completions: " << found_completions;
                handled_timeslice_callbacks_.call_async(found_completions);
                L_(trace) << "ts_completions_thread_ - call_async called" << found_completions;
            } else {
                L_(trace) << "no completions found ...";
                this_thread::sleep_for(chrono::milliseconds(500));
            }
        }
    });

}

uint64_t TsclientWriter::get_buffer_size() {
    return buffer_size_;
}

std::shared_ptr<char> TsclientWriter::get_buffer()  {
    return shared_ptr<char>(buffer_.get(), no_del(char));
}

void TsclientWriter::set_buffer_map(std::shared_ptr<BufferMap> buffer_map) {
    buffer_map_ = buffer_map;
}

void TsclientWriter::write_timeslice(std::vector<BufferMap::ListElement*>& elements) {
    time_point<high_resolution_clock> start;
    time_point<high_resolution_clock> stop;
    start = high_resolution_clock::now();
    vector<fles::TimesliceComponentDescriptor*> desc_ptr;
    vector<uint8_t*> data_ptr;
    // L_(trace) << "write_timeslice - ts_input_output_diff: " << ++ts_input_output_cnt_diff  ;
    uint64_t combined_size = 0;
    for (auto desc_it = elements.begin(); desc_it != elements.end(); ++desc_it) {
        auto *const descriptor_el = *desc_it;
        if (1 == (descriptor_el->tag >> (sizeof(uint16_t) * 8))) { // referencing a descriptor
            desc_ptr.push_back(reinterpret_cast<fles::TimesliceComponentDescriptor*>(buffer_.get() + descriptor_el->address));
            const auto idx = static_cast<uint16_t>(descriptor_el->tag);
            for (const auto& element : elements) {
                if (static_cast<uint16_t>(element->tag) == idx && 2 == (element->tag >> (sizeof(uint16_t) * 8))) { // is refere
                    combined_size += descriptor_el->len;
                    combined_size += element->len;
                    data_ptr.push_back(reinterpret_cast<uint8_t*>(buffer_.get() + element->address));
                    break;
                }
            }
        } // else referencing data
    }

    auto ts = make_shared<MyTimeslice>();
    ts_pos_++;

    ts->set_timeslice_descriptor({
        desc_ptr[0]->ts_num,
        ts_pos_, timeslice_size_,
        static_cast<uint32_t>(desc_ptr.size())
    });
    L_(debug) << "elements[0]->compontent_id: " << elements[0]->compontent_id << endl;

    ts->set_desc(std::move(desc_ptr));
    ts->set_data(std::move(data_ptr));
    {
        unique_lock<mutex> l(mtx_);
        tspos_componentid_map_[ts_pos_] = elements[0]->compontent_id;
        L_(debug) << "send_work_item - open completions: " << ts_input_output_cnt_diff_;
        ts_input_output_cnt_diff_++;
        ts_buffer_->send_work_item(ts);
    }
    L_(trace) << "Sending work item ... size: " << combined_size;
    stop = high_resolution_clock::now();

    L_(debug) << "TS writer - ts written after: " <<  duration_cast<milliseconds>(stop-start).count();
}

TsclientWriter::~TsclientWriter() {
    item_distributor_->stop();
    distributor_thread_.join();
}

bool TsclientWriter::pop_finished_component_id(uint64_t& component_id) {
    unique_lock<mutex> l(mtx_);

    if (!component_ids_done_.empty()) {
        component_id = component_ids_done_.front();
        L_(debug) << "TsclientWriter::pop_finished_component_id - component_id: " << component_id;
        component_ids_done_.pop();
        return true;
    }

    return false;
}

uint64_t TsclientWriter::get_finished_component_id_cnt() {
    unique_lock<mutex> l(mtx_);
    return component_ids_done_.size();
}