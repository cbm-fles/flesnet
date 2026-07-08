#pragma once

#include <cstdint>
#include <df/WorkItems/WorkItem.hpp>


enum WiType : uint64_t {
    WI_TYPES,
    wi_work_done,
    wi_buffer_full_report
};

class WiBufferFullReport : public WorkItem {
public:
    uint64_t node_id = 0; //!> node_id of which the buffer is full
    uint64_t group_id = 0; //!> group_id of which the buffer is full

    WiBufferFullReport() {
        type = static_cast<WorkItem::Type>(wi_buffer_full_report);
    }

    // Boost
    template<typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        WI_SERIALIZE(ar);
        ar & node_id & group_id;
    }

    std::shared_ptr<char> serialize(uint64_t *size) override {
        std::ostringstream sstream;
        boost::archive::text_oarchive archive(sstream);
        archive & *this;

        auto str = sstream.str();
        const char* cstr = str.c_str();
        *size = str.size() + 1;
        std::shared_ptr<char> shared_ptr(
            new char[*size],
            std::default_delete<char[]>()
        );
        std::copy(cstr, cstr + *size, shared_ptr.get());
        return shared_ptr;
    }

    bool deserialize(std::shared_ptr<char> serialized) override {
        std::string s(serialized.get());
        std::istringstream sstream(s);
        boost::archive::text_iarchive archive(sstream);
        archive & *this;
        return true;
    }
};

class WiWorkDone : public WorkItem {
public:
    WiWorkDone() {
        type = static_cast<WorkItem::Type>(wi_work_done);
    }

    // Boost
    template<typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        WI_SERIALIZE(ar);
    }

    std::shared_ptr<char> serialize(uint64_t *size) override {
        std::ostringstream sstream;
        boost::archive::text_oarchive archive(sstream);
        archive & *this;

        auto str = sstream.str();
        const char* cstr = str.c_str();
        *size = str.size() + 1;
        std::shared_ptr<char> shared_ptr(
            new char[*size],
            std::default_delete<char[]>()
        );
        std::copy(cstr, cstr + *size, shared_ptr.get());
        return shared_ptr;
    }

    bool deserialize(std::shared_ptr<char> serialized) override {
        std::string s(serialized.get());
        std::istringstream sstream(s);
        boost::archive::text_iarchive archive(sstream);
        archive & *this;
        return true;
    }
};