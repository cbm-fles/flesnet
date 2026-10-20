// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// Logging must not wait for its outputs: with nobody reading stderr, the
// threads that log carry on, and what the queue cannot take is dropped and
// counted. With the argument "exit", the process ends while nobody reads
// stderr, which must not take forever either. With "fatal", it logs a fatal
// message first, which must not wait forever for stderr.

#include "log.hpp"

#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

int main(int argc, char* argv[]) {
  std::string_view const mode = argc > 1 ? argv[1] : "";
  bool const exit_stalled = mode == "exit" || mode == "fatal";

  // stderr becomes a pipe that nobody reads for now
  std::array<int, 2> fds{-1, -1};
  if (pipe(fds.data()) != 0 || dup2(fds[1], STDERR_FILENO) < 0) {
    return EXIT_FAILURE;
  }
  close(fds[1]);

  logging::Options options;
  options.level = info;
  logging::setup(options);

  // More than the pipe and then the queue take. If logging waited for
  // stderr, this would not return and the test would time out. The errors
  // come first, while no thread is stuck writing yet.
  constexpr std::size_t n_errors = 2'000;
  constexpr std::size_t n_messages = 200'000;
  std::string const payload(1000, 'x');
  for (std::size_t i = 0; i < n_errors; ++i) {
    L_(error) << "stalled error " << i << " " << payload;
  }
  if (mode == "fatal") {
    L_(fatal) << "stalled fatal";
  }
  if (exit_stalled) {
    return EXIT_SUCCESS;
  }
  for (std::size_t i = 0; i < n_messages; ++i) {
    L_(info) << "stalled " << i << " " << payload;
  }

  std::size_t n_written = 0;
  std::size_t n_dropped = 0;
  auto count = [&](std::string_view line) {
    if (line.find("stalled ") != std::string_view::npos) {
      ++n_written;
    }
    // [HH:MM:SS] ERROR: 123 log messages dropped, ...
    auto const end = line.find(" log messages dropped");
    if (end != std::string_view::npos) {
      auto const begin = line.rfind(' ', end - 1) + 1;
      n_dropped += std::stoull(std::string(line.substr(begin, end - begin)));
    }
  };
  std::thread reader([&] {
    std::string text;
    std::array<char, 65536> buffer{};
    for (;;) {
      auto const n = read(fds[0], buffer.data(), buffer.size());
      if (n <= 0) {
        break;
      }
      text.append(buffer.data(), static_cast<std::size_t>(n));
      std::size_t begin = 0;
      for (auto end = text.find('\n'); end != std::string::npos;
           end = text.find('\n', begin)) {
        count(std::string_view(text).substr(begin, end - begin));
        begin = end + 1;
      }
      text.erase(0, begin);
    }
  });
  logging::flush();
  close(STDERR_FILENO); // the end of the pipe for the reader
  reader.join();

  std::cout << n_written << " messages written, " << n_dropped
            << " reported as dropped\n";
  bool const ok = n_written > 0 && n_dropped > 0 &&
                  n_written + n_dropped == n_errors + n_messages;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
