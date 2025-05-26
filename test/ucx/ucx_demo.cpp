#include "UcxConnection.hpp"
#include "UcxContext.hpp"
#include "UcxListener.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

constexpr uint16_t DEFAULT_PORT = 13337;
constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1MB test buffer
constexpr uint64_t TEST_TAG = 0x42;
constexpr int CONNECTION_TIMEOUT_MS = 5000;

std::atomic<bool> keep_running{true};

void signal_handler(int) {
  keep_running = false;
  std::cout << "Shutting down..." << std::endl;
}

void run_server(uint16_t port) {
  std::cout << "Starting UCX server on port " << port << std::endl;

  // Initialize UCX context
  UcxContext context;

  // Allocate and initialize a test buffer
  std::vector<uint8_t> send_buffer(BUFFER_SIZE, 0);
  std::vector<uint8_t> recv_buffer(BUFFER_SIZE, 0);

  for (size_t i = 0; i < BUFFER_SIZE; i++) {
    send_buffer[i] = static_cast<uint8_t>(i & 0xFF);
  }

  // Create connection manager to handle client connections
  UcxConnectionManager connection_manager(context, port, 1);

  std::cout << "Waiting for client connection..." << std::endl;
  bool connected =
      connection_manager.wait_for_all_connections(CONNECTION_TIMEOUT_MS);

  if (!connected) {
    std::cerr << "Timeout waiting for connection" << std::endl;
    return;
  }

  std::cout << "Client connected!" << std::endl;

  // Test completion flag
  std::atomic<bool> send_completed{false};
  std::atomic<bool> recv_completed{false};

  // Create memory region for RMA operations
  UcxMemoryRegion memory_region(context, send_buffer.data(), send_buffer.size(),
                                UCP_MEM_MAP_ALLOCATE);

  // The first client that connected
  UcxConnection connection(context, 0, 0);

  // Send a tagged message and wait for response
  std::cout << "Server sending " << send_buffer.size() << " bytes..."
            << std::endl;

  connection.send_tagged(send_buffer.data(), send_buffer.size(), TEST_TAG,
                         [&send_completed](ucs_status_t status) {
                           send_completed = true;
                           if (status != UCS_OK) {
                             std::cerr << "Send failed with status " << status
                                       << std::endl;
                           }
                         });

  // Receive response from client
  connection.recv_tagged(recv_buffer.data(), recv_buffer.size(), TEST_TAG,
                         [&recv_completed](ucs_status_t status) {
                           recv_completed = true;
                           if (status != UCS_OK) {
                             std::cerr << "Receive failed with status "
                                       << status << std::endl;
                           }
                         });

  // Progress until both operations complete or timeout
  auto start_time = std::chrono::steady_clock::now();
  while ((!send_completed || !recv_completed) && keep_running) {
    context.progress();

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time)
            .count();

    if (elapsed > CONNECTION_TIMEOUT_MS) {
      std::cerr << "Timeout waiting for operations to complete" << std::endl;
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Verify data
  bool data_valid = true;
  for (size_t i = 0; i < BUFFER_SIZE && data_valid; i++) {
    if (recv_buffer[i] != send_buffer[i]) {
      std::cerr << "Data mismatch at index " << i << ": expected "
                << static_cast<int>(send_buffer[i]) << ", got "
                << static_cast<int>(recv_buffer[i]) << std::endl;
      data_valid = false;
    }
  }

  if (data_valid) {
    std::cout << "Data verification successful!" << std::endl;
  } else {
    std::cerr << "Data verification failed!" << std::endl;
  }

  std::cout << "Server test completed" << std::endl;
}

void run_client(const std::string& server, uint16_t port) {
  std::cout << "Starting UCX client, connecting to " << server << ":" << port
            << std::endl;

  // Initialize UCX context
  UcxContext context;

  // Create connection
  UcxConnection connection(context, 0, 0);

  try {
    // Connect to server
    connection.connect(server, port);
    std::cout << "Connected to server!" << std::endl;

    // Allocate test buffers
    std::vector<uint8_t> send_buffer(BUFFER_SIZE, 0);
    std::vector<uint8_t> recv_buffer(BUFFER_SIZE, 0);

    for (size_t i = 0; i < BUFFER_SIZE; i++) {
      send_buffer[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Test completion flags
    std::atomic<bool> send_completed{false};
    std::atomic<bool> recv_completed{false};

    // Receive data from server
    connection.recv_tagged(recv_buffer.data(), recv_buffer.size(), TEST_TAG,
                           [&recv_completed](ucs_status_t status) {
                             recv_completed = true;
                             if (status != UCS_OK) {
                               std::cerr << "Receive failed with status "
                                         << status << std::endl;
                             }
                           });

    // Send data back to server
    connection.send_tagged(send_buffer.data(), send_buffer.size(), TEST_TAG,
                           [&send_completed](ucs_status_t status) {
                             send_completed = true;
                             if (status != UCS_OK) {
                               std::cerr << "Send failed with status " << status
                                         << std::endl;
                             }
                           });

    // Progress until both operations complete or timeout
    auto start_time = std::chrono::steady_clock::now();
    while ((!send_completed || !recv_completed) && keep_running) {
      context.progress();

      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - start_time)
                         .count();

      if (elapsed > CONNECTION_TIMEOUT_MS) {
        std::cerr << "Timeout waiting for operations to complete" << std::endl;
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Verify data
    bool data_valid = true;
    for (size_t i = 0; i < BUFFER_SIZE && data_valid; i++) {
      if (recv_buffer[i] != static_cast<uint8_t>(i & 0xFF)) {
        std::cerr << "Data mismatch at index " << i << ": expected "
                  << static_cast<int>(i & 0xFF) << ", got "
                  << static_cast<int>(recv_buffer[i]) << std::endl;
        data_valid = false;
      }
    }

    if (data_valid) {
      std::cout << "Data verification successful!" << std::endl;
    } else {
      std::cerr << "Data verification failed!" << std::endl;
    }

    // Clean disconnect
    connection.disconnect();

  } catch (const UcxException& e) {
    std::cerr << "UCX error: " << e.what() << std::endl;
  }

  std::cout << "Client test completed" << std::endl;
}

void print_usage(const char* progname) {
  std::cout << "Usage: " << progname << " [server|client <host>] [port]"
            << std::endl;
  std::cout << "  server        - Run in server mode" << std::endl;
  std::cout << "  client <host> - Run in client mode, connecting to <host>"
            << std::endl;
  std::cout << "  port          - Optional port number (default: "
            << DEFAULT_PORT << ")" << std::endl;
}

int main(int argc, char* argv[]) {
  // Register signal handler
  signal(SIGINT, signal_handler);

  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  std::string mode = argv[1];
  uint16_t port = DEFAULT_PORT;

  // Parse port if provided
  if ((mode == "server" && argc > 2) || (mode == "client" && argc > 3)) {
    int port_arg_idx = (mode == "server") ? 2 : 3;
    try {
      port = static_cast<uint16_t>(std::stoi(argv[port_arg_idx]));
    } catch (...) {
      std::cerr << "Invalid port number: " << argv[port_arg_idx] << std::endl;
      return 1;
    }
  }

  try {
    if (mode == "server") {
      run_server(port);
    } else if (mode == "client") {
      if (argc < 3) {
        std::cerr << "Client mode requires a host argument" << std::endl;
        print_usage(argv[0]);
        return 1;
      }

      std::string host = argv[2];
      run_client(host, port);
    } else {
      std::cerr << "Unknown mode: " << mode << std::endl;
      print_usage(argv[0]);
      return 1;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
