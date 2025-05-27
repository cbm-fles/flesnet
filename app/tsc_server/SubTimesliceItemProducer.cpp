#include "SubTimesliceItemProducer.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <sstream>

void SubTimesliceItemProducer::send_work_item(fles::SubTimesliceDescriptor st) {
  // Serialize the SubTimesliceComponentDescriptor to a string
  std::ostringstream oss;
  {
    boost::archive::binary_oarchive oa(oss);
    oa << st;
  }
  // Send the serialized data as a work item
  uint64_t ts_id = st.start_time_ns / st.duration_ns;
  ItemProducer::send_work_item(ts_id, oss.str());
}

bool SubTimesliceItemProducer::try_receive_completion(
    fles::SubTimesliceCompletion& c) {
  ItemID id;
  if (ItemProducer::try_receive_completion(&id)) {
    c.ts_id = id;
    return true;
  }
  return false;
}

void SubTimesliceItemProducer::handle_completions() {
  fles::SubTimesliceCompletion c{};
  while (SubTimesliceItemProducer::try_receive_completion(c)) {
    if (c.ts_id == acked_) {
      do {
        ++acked_;
      } while (ack_.at(acked_) > c.ts_id);

      // loop over all builders and update with ack_before
      // for (auto&& builder : builders) {
      //  builder->ack_before(acked_ * timeslice_size_time);
      //}
    } else {
      ack_.at(c.ts_id) = c.ts_id;
    }
  }
}
