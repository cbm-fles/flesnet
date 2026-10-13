#pragma once

#include <cstdint>
#include <df/WorkItems/WorkItem.hpp>

enum WiType : uint64_t {
  WI_TYPES,
  wi_work_done,
  wi_timeslice_status,
  wi_buffer_full_report
};

class WiBufferFullReport : public WorkItem {
public:
  uint64_t node_id = 0;  //!> node_id of which the buffer is full
  uint64_t group_id = 0; //!> group_id of which the buffer is full

  WiBufferFullReport() {
    type = static_cast<WorkItem::Type>(buffer_status);
  }

  // Boost
  template <typename Archive>
  void serialize(Archive& ar, const unsigned /*version*/) {
    WI_SERIALIZE(ar);
    ar & node_id & group_id;
  }

  std::shared_ptr<char> serialize(uint64_t* size) override {
    std::ostringstream sstream;
    boost::archive::text_oarchive archive(sstream);
    archive&* this;

    auto str = sstream.str();
    const char* cstr = str.c_str();
    *size = str.size() + 1;
    std::shared_ptr<char> shared_ptr(
        new char[*size],
        std::default_delete<char[]>());
    std::copy(cstr, cstr + *size, shared_ptr.get());
    return shared_ptr;
  }

  bool deserialize(std::shared_ptr<char> serialized) override {
    std::string s(serialized.get());
    std::istringstream sstream(s);
    boost::archive::text_iarchive archive(sstream);
    archive&* this;
    return true;
  }
};

// Sent by a TS sender. Means a new timeslice is available
class WiTimesliceStatus : public WorkItem {
public:
  std::vector<uint64_t> ts_sizes;
  std::vector<uint64_t> ts_indexes;
  std::vector<uint64_t> node_uids;
  float buffer_level = 0;     // in percent
  float rem_buffer_level = 0; // in percent
  uint64_t vector_clock = 0;

  WiType sub_type;
  WiTimesliceStatus() {
    type = WorkItem::Type::buffer_status;
    sub_type = WiType::none;
  }

  // Boost
  template <typename Archive>
  void serialize(Archive& ar, const unsigned /*version*/) {
    WI_SERIALIZE(ar);
    ar &
        ts_sizes &
        ts_indexes &
        node_uids &
        buffer_level &
        rem_buffer_level &
        vector_clock &
        sub_type;
  }

  std::shared_ptr<char> serialize(uint64_t* size) override {
    std::ostringstream sstream;
    boost::archive::text_oarchive archive(sstream);
    archive&* this;

    auto str = sstream.str();
    const char* cstr = str.c_str();
    *size = str.size() + 1;
    std::shared_ptr<char> shared_ptr(
        new char[*size],
        std::default_delete<char[]>());
    std::copy(cstr, cstr + *size, shared_ptr.get());
    return shared_ptr;
  }

  bool deserialize(std::shared_ptr<char> serialized) override {
    std::string s(serialized.get());
    std::istringstream sstream(s);
    boost::archive::text_iarchive archive(sstream);
    archive&* this;
    return true;
  }
};
