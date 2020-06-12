#ifndef ZMQ_DEMO_EXAMPLEPRODUCER_HPP
#define ZMQ_DEMO_EXAMPLEPRODUCER_HPP

#include "ItemProducer.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

#include <zmq.hpp>

class ExampleProducer : public ItemProducer {
  constexpr static auto wait_time_ = std::chrono::milliseconds{500};

public:
  ExampleProducer(std::shared_ptr<zmq::context_t> context,
                  const std::string& distributor_address)
      : ItemProducer(context, distributor_address){};

  void operator()() {
    while (i_ < 5 || !outstanding_.empty()) {

      // receive completion messages if available
      while (true) {
        ItemID id;
        bool result = try_receive_completion(id);
        if (!result) {
          break;
        }
        std::cout << "Producer RELEASE item " << id << std::endl;
        if (outstanding_.erase(id) != 1) {
          std::cerr << "Error: invalid item " << id << std::endl;
        };
      }

      // wait some time
      std::this_thread::sleep_for(wait_time_);

      if (i_ < 5) {
        // send work item
        outstanding_.insert(i_);
        std::cout << "Producer GENERATE item " << i_ << std::endl;
        send_work_item(i_, "");
        ++i_;
      }
    }
    std::cout << "Producer DONE" << std::endl;
  }

private:
  size_t i_ = 0;
  std::set<size_t> outstanding_;
};

#endif
