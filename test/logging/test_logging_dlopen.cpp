// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// Checks that libraries loaded in different ways share one logging instance:
// linked directly (TESTLOG_LINKED), or via dlopen with RTLD_GLOBAL or
// RTLD_LOCAL, the latter without libfles_logging in the global scope.
//
// "local" loads b first, "local-late" loads a (and with it libfles_logging)
// first.

#include <dirent.h>
#ifndef TESTLOG_LINKED
#include <dlfcn.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

extern "C" {
using setup_fn = void (*)(char const*);
using log_fn = void (*)(int);
using flush_fn = void (*)();

#ifdef TESTLOG_LINKED
void testlog_a_setup(char const* filename);
void testlog_a_log(int i);
void testlog_a_flush();
void testlog_b_log(int i);
#endif
}

namespace {

struct Api {
  setup_fn setup;
  log_fn log_a;
  flush_fn flush;
  log_fn log_b;
};

#ifdef TESTLOG_LINKED
Api load(std::string const& /* mode */) {
  return {testlog_a_setup, testlog_a_log, testlog_a_flush, testlog_b_log};
}
#else
void* open(char const* path, int mode) {
  void* handle = dlopen(path, RTLD_NOW | mode);
  if (handle == nullptr) {
    std::cerr << "dlopen failed: " << dlerror() << "\n";
    std::exit(EXIT_FAILURE);
  }
  return handle;
}

template <typename T> T symbol(void* handle, char const* name) {
  void* s = dlsym(handle, name);
  if (s == nullptr) {
    std::cerr << "dlsym failed: " << dlerror() << "\n";
    std::exit(EXIT_FAILURE);
  }
  return reinterpret_cast<T>(s);
}

Api load(std::string const& mode) {
  int const flags = mode == "global" ? RTLD_GLOBAL : RTLD_LOCAL;
  void* a = nullptr;
  void* b = nullptr;
  if (mode == "local-late") {
    a = open(TESTLOG_A, flags);
    b = open(TESTLOG_B, flags);
  } else {
    // b first, so that its load-time message starts the writer in dlopen
    b = open(TESTLOG_B, flags);
    a = open(TESTLOG_A, flags);
  }
  return {symbol<setup_fn>(a, "testlog_a_setup"),
          symbol<log_fn>(a, "testlog_a_log"),
          symbol<flush_fn>(a, "testlog_a_flush"),
          symbol<log_fn>(b, "testlog_b_log")};
}
#endif

int count_lines(std::string const& filename, std::string const& text) {
  std::ifstream in(filename);
  int n = 0;
  for (std::string line; std::getline(in, line);) {
    if (line.find(text) != std::string::npos) {
      ++n;
    }
  }
  return n;
}

int count_threads(std::string const& name) {
  int n = 0;
  DIR* dir = opendir("/proc/self/task");
  while (dirent* entry = readdir(dir)) {
    if (entry->d_name[0] == '.') {
      continue;
    }
    std::ifstream comm(std::string("/proc/self/task/") +
                       static_cast<char const*>(entry->d_name) + "/comm");
    std::string comm_name;
    std::getline(comm, comm_name);
    if (comm_name == name) {
      ++n;
    }
  }
  closedir(dir);
  return n;
}

bool check(bool ok, std::string const& what) {
  std::cout << (ok ? "ok:     " : "FAILED: ") << what << "\n";
  return ok;
}

} // namespace

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0]
              << " linked|global|local|local-late FILE\n";
    return EXIT_FAILURE;
  }
  std::string const mode = argv[1];
  std::string const file = argv[2];

  Api const api = load(mode);
  std::remove(file.c_str()); // the log is appended to it
  api.setup(file.c_str());

  constexpr int n = 100;
  std::thread thread([&api] {
    for (int i = 0; i < n; ++i) {
      api.log_b(i);
    }
  });
  for (int i = 0; i < n; ++i) {
    api.log_a(i);
    api.log_b(n + i);
  }
  thread.join();
  api.flush();

  bool ok = true;
  ok &= check(count_threads("fles_logging") == 1, "one writer thread");
  ok &=
      check(count_lines(file, R"("message":"from a )") == n, "messages from a");
  ok &= check(count_lines(file, R"("message":"from b )") == 2 * n,
              "messages from b");
  ok &= check(count_lines(file, R"("message":"formatted from b )") == 2 * n,
              "formatted messages from b");
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
