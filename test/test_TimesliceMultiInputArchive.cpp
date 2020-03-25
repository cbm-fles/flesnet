// Copyright 2020 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
#define BOOST_TEST_MODULE test_TimesliceMultiInputArchive
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include "TimesliceMultiInputArchive.hpp"

BOOST_AUTO_TEST_CASE(archive_exception_test) {
  std::string filename("./does_not_exist.tsa");
  BOOST_CHECK_THROW(fles::TimesliceMultiInputArchive source(filename),
                    std::out_of_range);
}

BOOST_AUTO_TEST_CASE(reference_file_existence_test) {
  std::string filename("./example1.tsa");
  BOOST_CHECK_NO_THROW(fles::TimesliceMultiInputArchive source(filename));
}

BOOST_AUTO_TEST_CASE(reference_archive_test) {
  std::string filename("./example1.tsa;./example1.tsa");
  uint64_t count = 0;
  fles::TimesliceMultiInputArchive source(filename);
  while (auto timeslice = source.get()) {
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 4);
}

BOOST_AUTO_TEST_CASE(invalid_archive_test) {
  std::string filename("./example1.msa");
  BOOST_CHECK_THROW(fles::TimesliceMultiInputArchive source(filename),
                    std::runtime_error);
}
