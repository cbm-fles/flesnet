// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// A library may be unloaded right after it has logged: its messages, which
// are written later, must not refer to its memory.

#include "log.hpp"

#include <dlfcn.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  std::string const file = argv[1];
  std::remove(file.c_str()); // the log is appended to it
  logging::Options options;
  options.level = error; // keep stderr quiet
  options.file = file;
  options.file_level = info;
  logging::setup(options);

  constexpr int n = 3;
  for (int i = 0; i < n; ++i) {
    void* handle = dlopen(TESTLOG_A, RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
      std::cerr << "dlopen failed: " << dlerror() << "\n";
      return EXIT_FAILURE;
    }
    auto* log = reinterpret_cast<void (*)(int)>(dlsym(handle, "testlog_a_log"));
    if (log == nullptr) {
      std::cerr << "dlsym failed: " << dlerror() << "\n";
      return EXIT_FAILURE;
    }
    log(i);
    dlclose(handle);
    usleep(50'000); // longer than the writer thread lets messages wait
  }
  logging::flush();

  std::ifstream in(file);
  int n_lines = 0;
  for (std::string line; std::getline(in, line);) {
    if (line.find(R"("message":"from a )") != std::string::npos &&
        line.find(R"("file_name":"testlog_a.cpp")") != std::string::npos) {
      ++n_lines;
    }
  }
  return n_lines == n ? EXIT_SUCCESS : EXIT_FAILURE;
}
