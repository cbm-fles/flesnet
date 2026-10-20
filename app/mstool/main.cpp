// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "Parameters.hpp"
#include "log_short.hpp"
#include <cstdlib>
#include <exception>

int main(int argc, char* argv[]) {
  try {
    Parameters par(argc, argv);
    Application app(par);
    app.run();
  } catch (std::exception const& e) {
    FATAL("{}", e.what());
    return EXIT_FAILURE;
  }

  INFO("exiting");
  return EXIT_SUCCESS;
}
