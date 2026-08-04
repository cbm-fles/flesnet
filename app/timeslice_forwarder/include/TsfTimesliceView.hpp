#pragma once
#include <TimesliceView.hpp>

namespace tsforwarder {

class TimesliceView : public fles::TimesliceView  {

public:
  TimesliceView(
      std::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm,
      std::shared_ptr<const Item> work_item,
      const fles::TimesliceShmWorkItem& timeslice_item) :
        fles::TimesliceView{managed_shm, work_item, timeslice_item} {};

  TimesliceView(const TimesliceView&) = delete;
  void operator=(const TimesliceView&) = delete;
  ~TimesliceView() override = default;

  std::vector<uint8_t*>& get_data() {
    return data_ptr_;
  }

  std::vector<fles::TimesliceComponentDescriptor*>& get_desc() {
    return desc_ptr_;
  }
};

}
