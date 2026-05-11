#include "Application.hpp"
#include "Parameters.hpp"
#include "log.hpp"
#include <boost/algorithm/string.hpp>
#include <cstdlib>

int main(int argc, char* argv[]) {
  try {
    Parameters par(argc, argv);
    Application app(par);
    app.run();
  } catch (std::exception const& e) {
    L_(fatal) << e.what();
    return EXIT_FAILURE;
  }

  L_(info) << "exiting";
  return EXIT_SUCCESS;
}
