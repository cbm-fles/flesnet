#pragma once

#include "ItemProducer.hpp"
#include "RingBuffer.hpp"
#include "SubTimesliceDescriptor.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <cstddef>

namespace zmq {
class context_t;
}

class SubTimesliceItemProducer : public ItemProducer {
public:
  SubTimesliceItemProducer(zmq::context_t& context,
                           const std::string& distributor_address)
      : ItemProducer(context, distributor_address) {}

  void send_work_item(fles::SubTimesliceDescriptor st);

  bool try_receive_completion(fles::SubTimesliceCompletion& c);

  void handle_completions();

private:
  static constexpr size_t completion_buffer_size_exp = 20; // 1M elements
  RingBuffer<uint64_t, true> ack_{20};
  uint64_t acked_ = 0;

  // TOOD
  size_t timeslice_size_time = 100e6; // 100ms
};