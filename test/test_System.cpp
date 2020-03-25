// Copyright 2020 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
#define BOOST_TEST_MODULE test_System
#include <boost/test/unit_test.hpp>

#include "System.hpp"

BOOST_AUTO_TEST_CASE(glob_test) {
  std::string pattern = "*";
  auto v = fles::system::glob(pattern);
  BOOST_CHECK_GE(v.size(), 1);
}
