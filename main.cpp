#include "ExampleProducer.hpp"
#include "ExampleWorker.hpp"
#include "ItemDistributor.hpp"
#include "ItemWorkerProtocol.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <zmq.hpp>

int main() {
  auto zmq_context = std::make_shared<zmq::context_t>(1);

  std::string producer_address = "inproc://TEST";
  std::string worker_address = "ipc:///tmp/TEST_DELME";

  ItemDistributor distributor(zmq_context, producer_address, worker_address);
  std::thread distributor_thread(std::ref(distributor));

  ExampleProducer producer(zmq_context, producer_address);
  std::thread producer_thread(std::ref(producer));

  WorkerParameters param1{1, 0, WorkerQueuePolicy::QueueAll,
                          "example_client_1"};
  const auto delay1 = std::chrono::milliseconds{500};
  ExampleWorker worker1(worker_address, param1, delay1);
  std::thread worker1_thread(std::ref(worker1));

  WorkerParameters param2{1, 0, WorkerQueuePolicy::QueueAll,
                          "example_client_2"};
  const auto delay2 = std::chrono::milliseconds{500};
  ExampleWorker worker2(worker_address, param2, delay2);
  std::thread worker2_thread(std::ref(worker2));

  producer_thread.join();
  distributor.stop();
  distributor_thread.join();
  worker1_thread.join();
  worker2_thread.join();

  return 0;
}
