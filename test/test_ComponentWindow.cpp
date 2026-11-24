// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_ComponentWindow
#include <boost/test/unit_test.hpp>

#include "ComponentWindow.hpp"
#include "StorableMicroslice.hpp"
#include <deque>
#include <memory>
#include <vector>

using fles::ComponentAvailability;
using fles::ComponentWindow;

namespace {

std::vector<fles::MicrosliceDescriptor>
descriptors(uint64_t first_idx, uint64_t count, uint64_t step) {
  std::vector<fles::MicrosliceDescriptor> v(count);
  for (uint64_t i = 0; i < count; ++i) {
    v[i].idx = first_idx + i * step;
  }
  return v;
}

} // namespace

BOOST_AUTO_TEST_CASE(window_test) {
  ComponentWindow w = fles::component_window(4000, 1000, 100, 200);
  BOOST_CHECK_EQUAL(w.begin_ns, 3900);
  BOOST_CHECK_EQUAL(w.end_ns, 5200);
}

BOOST_AUTO_TEST_CASE(availability_test) {
  // microslices starting at 1000, 1100, ..., 2000
  auto v = descriptors(1000, 11, 100);
  std::vector<fles::MicrosliceDescriptor> empty;

  BOOST_CHECK(fles::component_availability(empty.begin(), empty.end(),
                                           ComponentWindow{1200, 1500}) ==
              ComponentAvailability::TryLater);

  // window starts before the first microslice
  BOOST_CHECK(fles::component_availability(v.begin(), v.end(),
                                           ComponentWindow{999, 1500}) ==
              ComponentAvailability::Failed);
  // window starts exactly at the first microslice
  BOOST_CHECK(fles::component_availability(v.begin(), v.end(),
                                           ComponentWindow{1000, 1500}) ==
              ComponentAvailability::Ok);

  // a microslice starting after the window end is needed
  BOOST_CHECK(fles::component_availability(v.begin(), v.end(),
                                           ComponentWindow{1200, 2000}) ==
              ComponentAvailability::TryLater);
  BOOST_CHECK(fles::component_availability(v.begin(), v.end(),
                                           ComponentWindow{1200, 1999}) ==
              ComponentAvailability::Ok);
}

BOOST_AUTO_TEST_CASE(range_test) {
  auto v = descriptors(1000, 11, 100);

  // the microslice covering the window start and all up to the end
  auto [first, last] =
      fles::component_range(v.begin(), v.end(), ComponentWindow{1250, 1600});
  BOOST_CHECK_EQUAL(first->idx, 1200);
  BOOST_CHECK_EQUAL(last->idx, 1600);
  BOOST_CHECK_EQUAL(std::distance(first, last), 4);

  // boundaries falling on microslice starts
  std::tie(first, last) =
      fles::component_range(v.begin(), v.end(), ComponentWindow{1200, 1650});
  BOOST_CHECK_EQUAL(first->idx, 1200);
  BOOST_CHECK_EQUAL(last->idx, 1700);

  BOOST_CHECK_THROW(
      fles::component_range(v.begin(), v.end(), ComponentWindow{999, 1500}),
      std::out_of_range);
  // the range ends at the newest microslice when it starts exactly at the
  // window end, and there is no end beyond that
  std::tie(first, last) =
      fles::component_range(v.begin(), v.end(), ComponentWindow{1200, 2000});
  BOOST_CHECK_EQUAL(last->idx, 2000);
  BOOST_CHECK_THROW(
      fles::component_range(v.begin(), v.end(), ComponentWindow{1200, 2001}),
      std::out_of_range);
}

BOOST_AUTO_TEST_CASE(release_point_test) {
  auto v = descriptors(1000, 11, 100);

  // the last microslice starting at or before the time is kept
  BOOST_CHECK_EQUAL(
      fles::component_release_point(v.begin(), v.end(), 1250)->idx, 1200);
  BOOST_CHECK_EQUAL(
      fles::component_release_point(v.begin(), v.end(), 1200)->idx, 1200);
  // nothing to release
  BOOST_CHECK(fles::component_release_point(v.begin(), v.end(), 500) ==
              v.begin());
  // everything older than the newest microslice is released
  BOOST_CHECK_EQUAL(
      fles::component_release_point(v.begin(), v.end(), 5000)->idx, 2000);
  std::vector<fles::MicrosliceDescriptor> empty;
  BOOST_CHECK(fles::component_release_point(empty.begin(), empty.end(), 5) ==
              empty.end());
}

BOOST_AUTO_TEST_CASE(projection_test) {
  // the functions work on any sequence that a projection maps to start times
  std::deque<std::unique_ptr<fles::StorableMicroslice>> q;
  for (uint64_t idx = 1000; idx <= 2000; idx += 100) {
    fles::MicrosliceDescriptor d = fles::MicrosliceDescriptor();
    d.idx = idx;
    q.push_back(
        std::make_unique<fles::StorableMicroslice>(d, std::vector<uint8_t>{}));
  }
  auto idx_of = [](const std::unique_ptr<fles::StorableMicroslice>& ms) {
    return ms->desc().idx;
  };

  BOOST_CHECK(fles::component_availability(
                  q.begin(), q.end(), ComponentWindow{1250, 1600}, idx_of) ==
              ComponentAvailability::Ok);
  auto [first, last] = fles::component_range(
      q.begin(), q.end(), ComponentWindow{1250, 1600}, idx_of);
  BOOST_CHECK_EQUAL((*first)->desc().idx, 1200);
  BOOST_CHECK_EQUAL((*last)->desc().idx, 1600);
  BOOST_CHECK_EQUAL(
      (*fles::component_release_point(q.begin(), q.end(), 1450, idx_of))
          ->desc()
          .idx,
      1400);
}
