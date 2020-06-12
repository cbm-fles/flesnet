#ifndef ZMQ_DEMO_EXAMPLEWORKER_HPP
#define ZMQ_DEMO_EXAMPLEWORKER_HPP

#include "ItemWorker.hpp"
#include "ItemWorkerProtocol.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include <zmq.hpp>

class ExampleWorker : public ItemWorker {
public:
  constexpr static auto average_wait_time_ = std::chrono::milliseconds{500};

  explicit ExampleWorker(const std::string& distributor_address)
      : ItemWorker(distributor_address) {
  };

  static void do_work(std::shared_ptr<const Item> item) {
    static std::default_random_engine eng{std::random_device{}()};
    static std::exponential_distribution<> dist(
        std::chrono::duration<double>(average_wait_time_).count());

    // Wait for a random time before completion
    std::this_thread::sleep_for(std::chrono::duration<double>{dist(eng)});
  }

  void operator()() {
    while (auto item = get()) {
      std::cout << "Worker received work item " << item->id() << std::endl;
      do_work(item);
      std::cout << "Worker finished work item " << item->id() << std::endl;
    }
  }
};

#endif
