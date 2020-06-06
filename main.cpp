#include "ItemDistributor.hpp"
#include "ItemProducer.hpp"
#include "ItemWorker.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
  std::string producer_address = "inproc://TEST";
  std::string worker_address = "ipc:///tmp/TEST_DELME";

  ItemDistributor distributor(producer_address, worker_address);
  std::thread distributor_thread(std::ref(distributor));

  ItemProducer producer(producer_address);
  std::thread producer_thread(std::ref(producer));

  ItemWorker worker1(worker_address);
  std::thread worker1_thread(std::ref(worker1));

  ItemWorker worker2(worker_address);
  std::thread worker2_thread(std::ref(worker2));

  producer_thread.join();
  distributor.stop();
  distributor_thread.join();
  worker1_thread.join();
  worker2_thread.join();

  return 0;
}
