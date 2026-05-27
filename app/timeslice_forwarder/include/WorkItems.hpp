#pragma once

#include <cstdint>
#include <df/WorkItems/WorkItem.hpp>


enum WiType : uint64_t {
    WI_TYPES,
    wi_buffer_request,
    wi_work_done
};

class WiBufferRequest : public WorkItem {
public:
    WiBufferRequest() {
        type = static_cast<WorkItem::Type>(wi_buffer_request);
    }

    // Boost
    BOOST_SERIALIZATION_SPLIT_MEMBER()
    template<class Archive>
    // Boost
    void save(Archive & ar, const unsigned int /*version*/) const {
        WI_SERIALIZE(ar);
    }

    // Boost
    template<class Archive>
    void load(Archive & ar, const unsigned int /*version*/) {
        WI_SERIALIZE(ar);
    }

    std::shared_ptr<char> serialize(uint64_t *size) override {
        std::ostringstream sstream;
        boost::archive::text_oarchive archive(sstream);
        archive & *this;
        *size = sstream.str().size() + 1;
        return {strdup(sstream.str().c_str()), std::default_delete<char>()};
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
    BOOST_SERIALIZATION_SPLIT_MEMBER()
    template<class Archive>
    // Boost
    void save(Archive & ar, const unsigned int /*version*/) const {
        WI_SERIALIZE(ar);
    }

    // Boost
    template<class Archive>
    void load(Archive & ar, const unsigned int /*version*/) {
        WI_SERIALIZE(ar);
    }

    std::shared_ptr<char> serialize(uint64_t *size) override {
        std::ostringstream sstream;
        boost::archive::text_oarchive archive(sstream);
        archive & *this;
        *size = sstream.str().size() + 1;
        return {strdup(sstream.str().c_str()), std::default_delete<char>()};
    }

    bool deserialize(std::shared_ptr<char> serialized) override {
        std::string s(serialized.get());
        std::istringstream sstream(s);
        boost::archive::text_iarchive archive(sstream);
        archive & *this;
        return true;
    }
};