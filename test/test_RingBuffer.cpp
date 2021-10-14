// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

#include "RingBuffer.hpp"
#include <iostream>

class Simple {
public:
  explicit Simple(int n = 5) : i(n) { std::cout << "Simple()\n"; }
  ~Simple() { std::cout << "~Simple()\n"; }
  int i;
};

int main() {
  try {
    RingBuffer<Simple, false, true> s;
    s.alloc_with_size(4);
    std::printf("ptr: %p\n", static_cast<void*>(s.ptr()));
  } catch (std::exception const& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
