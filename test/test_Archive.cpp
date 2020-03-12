// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_Timeslice
#include <boost/test/unit_test.hpp>

#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceOutputArchive.hpp"

BOOST_AUTO_TEST_CASE(timeslice_output_archive_sequence_test) {
  fles::TimesliceInputArchiveLoop source("example1.tsa", 3);
  fles::TimesliceOutputArchiveSequence sink("test2_%n.tsa", 2);
  uint64_t count = 0;
  while (auto timeslice = source.get()) {
    std::shared_ptr<const fles::Timeslice> ts(std::move(timeslice));
    sink.put(ts);
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 6);
}

BOOST_AUTO_TEST_CASE(timeslice_input_archive_sequence_test) {
  fles::TimesliceInputArchiveSequence source("test2_%n.tsa");
  fles::TimesliceOutputArchive sink("test3.tsa");
  uint64_t count = 0;
  while (auto timeslice = source.get()) {
    std::shared_ptr<const fles::Timeslice> ts(std::move(timeslice));
    sink.put(ts);
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 6);
}

BOOST_AUTO_TEST_CASE(microslice_output_archive_sequence_test) {
  fles::MicrosliceInputArchiveLoop source("example2.msa", 2);
  fles::MicrosliceOutputArchiveSequence sink("test3_%n.msa", 5);
  uint64_t count = 0;
  while (auto microslice = source.get()) {
    std::shared_ptr<const fles::Microslice> ts(std::move(microslice));
    sink.put(ts);
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 8);
}

BOOST_AUTO_TEST_CASE(microslice_input_archive_sequence_test) {
  fles::MicrosliceInputArchiveSequence source("test3_%n.msa");
  fles::MicrosliceOutputArchive sink("test4.msa");
  uint64_t count = 0;
  while (auto microslice = source.get()) {
    std::shared_ptr<const fles::Microslice> ts(std::move(microslice));
    sink.put(ts);
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 8);
}
