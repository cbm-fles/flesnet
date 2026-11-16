// Copyright 2020 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
#define BOOST_TEST_MODULE test_System
#include <boost/test/unit_test.hpp>

#include "System.hpp"

BOOST_AUTO_TEST_CASE(glob_test) {
  std::string pattern = "*";
  auto v = fles::system::glob(pattern);
  BOOST_CHECK_GE(v.size(), 1);
}

BOOST_AUTO_TEST_CASE(parse_time_point_test) {
  using fles::system::parse_time_point_ns;
  BOOST_CHECK_EQUAL(parse_time_point_ns("0"), 0);
  BOOST_CHECK_EQUAL(parse_time_point_ns("1763294400000000000"),
                    1763294400000000000LL);
  BOOST_CHECK_EQUAL(parse_time_point_ns("-5"), -5);
  // 2026-11-16T12:00:00Z
  BOOST_CHECK_EQUAL(parse_time_point_ns("2026-11-16T12:00:00Z"),
                    1794830400000000000LL);
  BOOST_CHECK_EQUAL(parse_time_point_ns("2026-11-16 12:00:00.5Z"),
                    1794830400500000000LL);
  BOOST_CHECK_EQUAL(parse_time_point_ns("2026-11-16T12:00:00.123456789Z"),
                    1794830400123456789LL);
  BOOST_CHECK_EQUAL(parse_time_point_ns("2026-11-16T13:00:00+01:00"),
                    1794830400000000000LL);
  BOOST_CHECK_EQUAL(parse_time_point_ns("2026-11-16T11:00:00-01"),
                    1794830400000000000LL);
  BOOST_CHECK_EQUAL(parse_time_point_ns("2026-11-16Z"),
                    1794830400000000000LL - 12 * 3600 * 1000000000LL);
  BOOST_CHECK_THROW(parse_time_point_ns(""), std::invalid_argument);
  BOOST_CHECK_THROW(parse_time_point_ns("-"), std::invalid_argument);
  BOOST_CHECK_THROW(parse_time_point_ns("yesterday"), std::invalid_argument);
  BOOST_CHECK_THROW(parse_time_point_ns("2026-11-16T12:00:00Zx"),
                    std::invalid_argument);
  BOOST_CHECK_THROW(parse_time_point_ns("2026-11-16T12:00:00.Z"),
                    std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(format_time_point_test) {
  using fles::system::format_time_point_ns;
  BOOST_CHECK_EQUAL(format_time_point_ns(0), "1970-01-01T00:00:00.000000000Z");
  BOOST_CHECK_EQUAL(format_time_point_ns(1794830400123456789LL),
                    "2026-11-16T12:00:00.123456789Z");
  BOOST_CHECK_EQUAL(format_time_point_ns(-1), "1969-12-31T23:59:59.999999999Z");
}

BOOST_AUTO_TEST_CASE(data_clock_test) {
  fles::system::DataClock clock;
  BOOST_CHECK(clock.is_system_clock());
  BOOST_CHECK_EQUAL(clock.to_data_time_ns(12345), 12345);
  BOOST_CHECK_EQUAL(clock.to_real_time_ns(12345), 12345);

  // data time 1000 at system time 1'000'000, half speed
  clock = {1000, 1000000, 0.5};
  BOOST_CHECK(!clock.is_system_clock());
  BOOST_CHECK_EQUAL(clock.to_data_time_ns(1000000), 1000);
  BOOST_CHECK_EQUAL(clock.to_data_time_ns(1002000), 2000);
  BOOST_CHECK_EQUAL(clock.to_data_time_ns(0), 1000 - 500000);
  BOOST_CHECK_EQUAL(clock.to_real_time_ns(2000), 1002000);
  BOOST_CHECK_EQUAL(clock.real_duration(500).count(), 1000);

  // the process clock never goes below zero
  fles::system::set_data_clock(
      {0, fles::system::current_real_time_ns() + 1000000000000LL, 1.0});
  BOOST_CHECK_EQUAL(fles::system::current_time_ns(), 0);
  fles::system::set_data_clock({});
  BOOST_CHECK(fles::system::data_clock().is_system_clock());
}
