#ifndef ZMQ_DEMO_EXAMPLEWORKER_HPP
#define ZMQ_DEMO_EXAMPLEWORKER_HPP

#include "ItemWorker.hpp"
#include "ItemWorkerProtocol.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include <zmq.hpp>

class ExampleWorker : public ItemWorker {
public:
  ExampleWorker(const std::string& distributor_address,
                WorkerParameters parameters,
                std::chrono::milliseconds constant_delay,
                std::chrono::milliseconds random_delay)
      : ItemWorker(distributor_address, std::move(parameters)),
        constant_delay_(constant_delay), random_delay_(random_delay){};

  void do_work(std::shared_ptr<const Item> item) {
    item_history_.push_back(item->id());
    // Wait for a random time before completion
    wait();
  }

  void operator()() {
    while (auto item = get()) {
      std::cout << "Worker " << parameters().client_name
                << " received work item " << item->id() << std::endl;
      do_work(item);
      std::cout << "Worker " << parameters().client_name
                << " finished work item " << item->id() << std::endl;
    }
    std::cout << "Worker " << parameters().client_name << " stopped, items:\n";
    for (auto const& id : item_history_) {
      std::cout << " " << id;
    }
    std::cout << std::endl;
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
    std::cout << "Worker " << parameters().client_name << " waiting for "
              << delay.count() << " ms" << std::endl;
    std::this_thread::sleep_for(delay);
  }

  const std::chrono::milliseconds constant_delay_;
  const std::chrono::milliseconds random_delay_;

  std::vector<ItemID> item_history_;
};

#endif
