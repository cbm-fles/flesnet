// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// Time spent in the calling thread per log message, with a file output.

#include "log.hpp"

#include <chrono>
#include <cstdio>
#include <functional>
#include <ratio>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int n_messages = 200000;

double run(int n_threads, std::function<void(int)> const& log) {
  std::vector<double> ns_per_message(static_cast<std::size_t>(n_threads));
  std::vector<std::thread> threads;
  for (int t = 0; t < n_threads; ++t) {
    threads.emplace_back([&, t] {
      auto const start = std::chrono::steady_clock::now();
      for (int i = 0; i < n_messages; ++i) {
        log(i);
      }
      auto const end = std::chrono::steady_clock::now();
      ns_per_message[static_cast<std::size_t>(t)] =
          std::chrono::duration<double, std::nano>(end - start).count() /
          n_messages;
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  double sum = 0;
  for (double const ns : ns_per_message) {
    sum += ns;
  }
  return sum / n_threads;
}

void bench(char const* name, std::function<void(int)> const& log) {
  run(8, log); // warm up, e.g. let the queues grow
  logging::flush();
  for (int const n_threads : {1, 8}) {
    auto const start = std::chrono::steady_clock::now();
    double const ns = run(n_threads, log);
    logging::flush();
    auto const end = std::chrono::steady_clock::now();
    double const total_s = std::chrono::duration<double>(end - start).count();
    std::printf("%-9s %d thread(s): %8.1f ns/message in caller, %6.2f s "
                "until written\n",
                name, n_threads, ns, total_s);
  }
}

} // namespace

int main(int argc, char* argv[]) {
  std::string const file = argc > 1 ? argv[1] : "bench_logging.log";
  logging::Options options;
  options.level = fatal; // keep stderr quiet
  options.file_level = info;
  options.file = file;
  logging::setup(options);

  double const value = 3.14159;
  bench("L_", [value](int i) {
    L_(info) << "bench message " << i << " value " << value;
  });
  bench("LF",
        [value](int i) { LF(info, "bench message {} value {}", i, value); });
  bench("LF+fields", [value](int i) {
    LF(info, "bench message {} value {}", i, value, logging::field("i", i),
       logging::field("value", value));
  });
  bench("LF_TS", [value](int i) {
    LF_TS(info, 17900000000 + i, "bench message {} value {}", i, value);
  });
  return 0;
}
