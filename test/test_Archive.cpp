// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_Archive
#include <boost/test/unit_test.hpp>

#include "MergingSource.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimesliceSource.hpp"

#include <memory>

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

BOOST_AUTO_TEST_CASE(timeslice_input_archive_sequence_vector_test) {
  fles::TimesliceInputArchiveSequence source(
      {"test2_0000.tsa", "test2_0001.tsa", "test2_0002.tsa"});
  uint64_t count = 0;
  while (auto timeslice = source.get()) {
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 6);
}

BOOST_AUTO_TEST_CASE(timeslice_input_archive_sequence_vector_test2) {
  fles::TimesliceInputArchiveSequence source(
      {"test2_0000.tsa", "test2_0001.tsa", "test2_0002.tsa", "test2_0003.tsa"});
  uint64_t count = 0;
  BOOST_CHECK_THROW(
      while (auto timeslice = source.get()) { ++count; },
      std::ios_base::failure);
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

BOOST_AUTO_TEST_CASE(merging_input_archive_test) {
  std::unique_ptr<fles::TimesliceSource> source0 =
      std::make_unique<fles::TimesliceInputArchiveSequence>("test2_%n.tsa");
  std::unique_ptr<fles::TimesliceSource> source1 =
      std::make_unique<fles::TimesliceInputArchive>("example1.tsa");
  std::vector<std::unique_ptr<fles::TimesliceSource>> sources;
  sources.emplace_back(std::move(source0));
  sources.emplace_back(std::move(source1));
  fles::MergingSource<fles::TimesliceSource> merging_source{std::move(sources)};
  uint64_t count = 0;
  while (auto timeslice = merging_source.get()) {
    ++count;
  }
  BOOST_CHECK_EQUAL(count, 8);
}
