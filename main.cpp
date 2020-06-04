#include "MemoryItemClient.hpp"
#include "MemoryItemServer.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main(void)
{
  MemoryItemServer server("ipc:///tmp/TEST_DELME");

  std::thread server_thread(std::ref(server));

  {
    MemoryItemClient client("ipc:///tmp/TEST_DELME");
    client.run();
  }

  server.stop();
  server_thread.join();

  return 0;
}