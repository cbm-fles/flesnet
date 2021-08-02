// Copyright 2021 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_TimesliceAutoSource
#include <boost/test/unit_test.hpp>

#include "TimesliceAutoSource.hpp"

#include <memory>

BOOST_AUTO_TEST_CASE(input_archive_sequence_test) {
  fles::TimesliceAutoSource source("test2_%n.tsa");
  uint64_t count = 0;
  while (auto timeslice = source.get()) {
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 6);
}

BOOST_AUTO_TEST_CASE(input_archive_sequence_vector_test) {
  fles::TimesliceAutoSource source(
      {"test2_0000.tsa", "test2_0001.tsa", "test2_0002.tsa"});
  uint64_t count = 0;
  while (auto timeslice = source.get()) {
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 6);
}

BOOST_AUTO_TEST_CASE(input_archive_sequence_vector_test2) {
  BOOST_CHECK_THROW(
      fles::TimesliceAutoSource source({"test2_0000.tsa", "test2_0001.tsa",
                                        "test2_0002.tsa", "test2_0003.tsa"}),
      std::runtime_error);
}

BOOST_AUTO_TEST_CASE(merging_input_archive_test) {
  fles::TimesliceAutoSource source("test2_%n.tsa;example1.tsa");
  uint64_t count = 0;
  while (auto timeslice = source.get()) {
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 8);
}

BOOST_AUTO_TEST_CASE(invalid_input_archive_test) {
  std::string filename("./example1.msa");
  BOOST_CHECK_THROW(fles::TimesliceAutoSource source(filename),
                    std::runtime_error);
}
