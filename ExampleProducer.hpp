#ifndef ZMQ_DEMO_EXAMPLEPRODUCER_HPP
#define ZMQ_DEMO_EXAMPLEPRODUCER_HPP

#include "ItemProducer.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <thread>

#include <zmq.hpp>

class ExampleProducer : public ItemProducer {
public:
  ExampleProducer(std::shared_ptr<zmq::context_t> context,
                  const std::string& distributor_address,
                  std::chrono::milliseconds constant_delay,
                  std::chrono::milliseconds random_delay,
                  std::size_t item_count)
      : ItemProducer(std::move(context), distributor_address),
        constant_delay_(constant_delay), random_delay_(random_delay),
        item_count_limit_(item_count){};

  void operator()() {
    while (i_ < item_count_limit_ || !outstanding_.empty()) {

      // receive completion messages if available
      while (true) {
        ItemID id;
        bool result = try_receive_completion(&id);
        if (!result) {
          break;
        }
        std::cout << "Producer RELEASE item " << id << std::endl;
        if (outstanding_.erase(id) != 1) {
          std::cerr << "Error: invalid item " << id << std::endl;
        };
      }

      wait();

      if (i_ < item_count_limit_) {
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
  void wait() {
    auto delay = constant_delay_;
    if (random_delay_.count() != 0) {
      static std::default_random_engine eng{std::random_device{}()};
      static std::exponential_distribution<> dist(
          1.0 / std::chrono::duration<double>(random_delay_).count());
      auto this_random_delay = std::chrono::duration<double>{dist(eng)};
      delay += std::chrono::duration_cast<std::chrono::milliseconds>(
          this_random_delay);
    }
    std::cout << "Producer waiting for " << delay.count() << " ms" << std::endl;
    std::this_thread::sleep_for(delay);
  }

  size_t i_ = 0;
  std::set<size_t> outstanding_;
  const std::chrono::milliseconds constant_delay_;
  const std::chrono::milliseconds random_delay_;
  const std::size_t item_count_limit_ = 5;
};

#endif
