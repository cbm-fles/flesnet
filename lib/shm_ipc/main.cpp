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
  using namespace std::literals;
  auto zmq_context = std::make_shared<zmq::context_t>(1);

  std::string producer_address = "inproc://TEST";
  std::string worker_address = "ipc:///tmp/TEST_DELME";

  auto distributor = std::make_unique<ItemDistributor>(
      zmq_context, producer_address, worker_address);
  std::thread distributor_thread(std::ref(*distributor));

  // The producer creates items at a constant primary rate of "d0".
  const auto d0 = 500ms;
  const size_t item_count = 10;
  auto producer = std::make_unique<ExampleProducer>(
      zmq_context, producer_address, d0, 0ms, item_count);
  std::thread producer_thread(std::ref(*producer));

  // The "storage" worker is slightly faster than data generation. It never
  // skips events but creates backpressure instead.
  const WorkerParameters param1{1, 0, WorkerQueuePolicy::QueueAll, "storage"};
  ExampleWorker worker1(worker_address, param1, d0 / 2, d0 / 4);
  std::thread worker1_thread(std::ref(worker1));

  // The "fast_analysis" worker is about as fast as data generation, but
  // includes a random time component. It may skip events sometimes.
  const WorkerParameters param2{1, 0, WorkerQueuePolicy::PrebufferOne,
                                "fast_analysis"};
  ExampleWorker worker2(worker_address, param2, d0 / 2, d0 / 2);
  std::thread worker2_thread(std::ref(worker2));

  // The "slow_analysis" worker is much slower than data generation. It always
  // waits for the newest dataset and skips all other items during processing.
  const WorkerParameters param3{1, 0, WorkerQueuePolicy::Skip, "slow_analysis"};
  ExampleWorker worker3(worker_address, param3, d0 * 2, d0 * 2);
  std::thread worker3_thread(std::ref(worker3));

  // The "sampling_analysis" worker receives only 1/5 of the data. It should be
  // fast enough, but it is not allowed to generate backpressure if not.
  const WorkerParameters param4{5, 0, WorkerQueuePolicy::PrebufferOne,
                                "sampling_analysis"};
  ExampleWorker worker4(worker_address, param4, d0 * 3, d0);
  std::thread worker4_thread(std::ref(worker4));

  // The "offset_sampler" worker receives only 1/4 of the data. It is also
  // not fast enough, and it is not allowed to generate backpressure.
  const WorkerParameters param5{2, 1, WorkerQueuePolicy::PrebufferOne,
                                "offset_sampler"};
  ExampleWorker worker5(worker_address, param5, d0 * 4, d0 * 1);
  std::thread worker5_thread(std::ref(worker5));

  // Wait until producer has finished
  producer_thread.join();
  distributor->stop();
  distributor_thread.join();
  producer = nullptr;
  distributor = nullptr;

  // Elasticity test: add a worker
  const WorkerParameters param6{1, 0, WorkerQueuePolicy::PrebufferOne,
                                "fast_analysis_2"};
  ExampleWorker worker6(worker_address, param6, 50ms, 0ms);
  std::thread worker6_thread(std::ref(worker6));

  // Elasticity test: restart producer and distributor
  distributor = std::make_unique<ItemDistributor>(zmq_context, producer_address,
                                                  worker_address);
  std::thread distributor2_thread(std::ref(*distributor));

  producer = std::make_unique<ExampleProducer>(zmq_context, producer_address,
                                               d0, 0ms, item_count);
  std::thread producer2_thread(std::ref(*producer));

  // Wait until producer2 has finished
  producer2_thread.join();
  distributor->stop();
  distributor2_thread.join();

  worker1.stop();
  worker2.stop();
  worker3.stop();
  worker4.stop();
  worker5.stop();
  worker6.stop();
  worker1_thread.join();
  worker2_thread.join();
  worker3_thread.join();
  worker4_thread.join();
  worker5_thread.join();
  worker6_thread.join();

  return 0;
}
