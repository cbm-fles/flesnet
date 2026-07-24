#include "TimesliceWriter.hpp"


TimesliceWriter::TimesliceWriter(std::string address) {
    fles::ArchiveCompression compression = fles::ArchiveCompression::None;
    ts_sink_ = std::make_unique<MyTimesliceArchive>(
        address, compression);
}

uint64_t TimesliceWriter::get_buffer_size() {
    return 0;
}

void TimesliceWriter::write_timeslice(std::vector<BufferMap::ListElement*>& elements) {
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
    auto ts = std::make_shared<MyTimeslice>();
    ts->set_timeslice_descriptor({
        desc_ptr[0]->ts_num,
        0, 100,
        static_cast<uint32_t>(desc_ptr.size())
    });
    ts->set_desc(desc_ptr);
    ts->set_data(data_ptr);
    ts_sink_->put(std::reinterpret_pointer_cast<fles::StorableTimeslice>(ts));
}

void TimesliceWriter::set_buffer(std::shared_ptr<char> buffer) {
    buffer_ = buffer;
}

std::shared_ptr<char> TimesliceWriter::get_buffer() {
    return buffer_;
}

TimesliceWriter::~TimesliceWriter(){};
