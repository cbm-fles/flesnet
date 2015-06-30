#include <iostream>

#include <flib.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wunused-parameter"

using namespace flib;

flib_device_flesin* MyFlib = NULL;

int main(int argc, char *argv[])
{
  MyFlib = new flib_device_flesin(0);

  std::cout << MyFlib->print_build_info() << std::endl;
  std::cout << MyFlib->print_devinfo() << std::endl;

  if (MyFlib)
    delete MyFlib;
  return 0;
}
#pragma GCC diagnostic pop
