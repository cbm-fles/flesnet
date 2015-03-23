#include <iostream>

#include <flib.h>

using namespace flib2;

flib_device* MyFlib = NULL;

int main(int argc, char *argv[])
{
  MyFlib = new flib_device(0);

  std::cout << MyFlib->print_build_info() << std::endl;
  std::cout << MyFlib->print_devinfo() << std::endl;

  if (MyFlib)
    delete MyFlib;
  return 0;
}
