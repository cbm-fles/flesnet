#include "TsclientWriter.hpp"
#include "Utility.hpp"

using namespace std;

bool TsclientWriter::handle_timeslice_completions() {
    fles::TimesliceCompletion c{};
    bool found_completion = false;
    while (ts_buffer_->try_receive_completion(c)) {
        found_completion = true;
    }
    return found_completion;
}

TsclientWriter::TsclientWriter(std::string shm_uri) {
    UriComponents uri{shm_uri};
    uint32_t datasize = 27; // 128 MiB
    uint32_t descsize = 19; // 16 MiB
    uint32_t num_components = 26;
    const auto shm_identifier = uri.path;
    cout << "shm_identifier: " << uri.path << endl;
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

    // cout << "shm_identifier: " << shm_identifier << endl;
    producer_address_ = "inproc://" + shm_identifier;
    worker_address_ = "ipc://@" + shm_identifier;


    item_distributor_ = make_unique<ItemDistributor>(zmq_context_, producer_address_, worker_address_),
    ts_buffer_ = make_shared<FragmentedTimesliceBuffer>(zmq_context_, producer_address_, shm_identifier, datasize, descsize, num_components);
    // managed_timeslice_buffer = make_shared<ManagedTimesliceBuffer>(zmq_context_, shm_identifier, datasize, descsize, num_components);
    distributor_thread_ = thread(ref(*(item_distributor_.get())));
    buffer_ = shared_ptr<char>(static_cast<char*>(ts_buffer_->managed_shm_->get_address()));
    buffer_size_ = ts_buffer_->managed_shm_->get_size();
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
    vector<fles::TimesliceComponentDescriptor*> desc_ptr;
    vector<uint8_t*> data_ptr;
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

    auto ts = make_shared<MyTimeslice>();
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

TsclientWriter::~TsclientWriter() {
    item_distributor_->stop();
    distributor_thread_.join();
}
