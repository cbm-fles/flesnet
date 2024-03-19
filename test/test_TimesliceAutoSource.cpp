// Copyright 2021-2024 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_TimesliceAutoSource
#include "ArchiveDescriptor.hpp"
#include "TimesliceAutoSource.hpp"
#include <boost/serialization/access.hpp>
#include <boost/test/tools/old/interface.hpp>
#include <boost/test/unit_test.hpp>

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

// Minimal example for a class that can be used with AutoSource
class item {
public:
  int a;
  [[nodiscard]] int index() const { return a; };

private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar& a;
  }
};

BOOST_AUTO_TEST_CASE(item_auto_source_test) {
  using ItemAutoSource =
      fles::AutoSource<item, item, item, fles::ArchiveType::RecoResultsArchive>;

  ItemAutoSource source("test2_%n.tsa");
  BOOST_CHECK_EQUAL(source.eos(), true);
}
