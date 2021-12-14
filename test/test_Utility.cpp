// Copyright 2021 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
#define BOOST_TEST_MODULE test_Utility
#include <boost/test/unit_test.hpp>

#include "Utility.hpp"

BOOST_AUTO_TEST_CASE(split_test1) {
  const std::string str = "1&2&&3&45&";
  const auto v = split(str, "&");
  BOOST_CHECK_EQUAL(v.size(), 4);
  BOOST_CHECK_EQUAL(v.at(0), "1");
  BOOST_CHECK_EQUAL(v.at(1), "2");
  BOOST_CHECK_EQUAL(v.at(2), "3");
  BOOST_CHECK_EQUAL(v.at(3), "45");
}

BOOST_AUTO_TEST_CASE(split_test2) {
  const std::string str = "=1&2?&3&45";
  const auto v = split(str, "&?=");
  BOOST_CHECK_EQUAL(v.size(), 4);
  BOOST_CHECK_EQUAL(v.at(0), "1");
  BOOST_CHECK_EQUAL(v.at(1), "2");
  BOOST_CHECK_EQUAL(v.at(2), "3");
  BOOST_CHECK_EQUAL(v.at(3), "45");
}

BOOST_AUTO_TEST_CASE(uri_components_test1) {
  const std::string uri =
      "http://u:p@example.com:123/index?par1=1&par2=&par3&&#0";
  const auto c = UriComponents{uri};
  BOOST_CHECK_EQUAL(c.scheme, "http");
  BOOST_CHECK_EQUAL(c.authority, "u:p@example.com:123");
  BOOST_CHECK_EQUAL(c.path, "/index");
  BOOST_CHECK_EQUAL(c.query, "par1=1&par2=&par3&&");
  BOOST_CHECK_EQUAL(c.fragment, "0");
  BOOST_CHECK_EQUAL(c.query_components.size(), 3);
  BOOST_CHECK_EQUAL(c.query_components.at("par1"), "1");
  BOOST_CHECK_EQUAL(c.query_components.at("par2"), "");
  BOOST_CHECK_EQUAL(c.query_components.at("par3"), "");
  BOOST_CHECK_THROW(c.query_components.at("par4"), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(uri_components_test2) {
  const std::string uri = "file:///path/to/file";
  const auto c = UriComponents{uri};
  BOOST_CHECK_EQUAL(c.scheme, "file");
  BOOST_CHECK_EQUAL(c.authority, "");
  BOOST_CHECK_EQUAL(c.path, "/path/to/file");
  BOOST_CHECK_EQUAL(c.query, "");
  BOOST_CHECK_EQUAL(c.fragment, "");
}

BOOST_AUTO_TEST_CASE(uri_components_test3) {
  const auto c = UriComponents{"example.com"};
  BOOST_CHECK_EQUAL(c.scheme, "");
  BOOST_CHECK_EQUAL(c.authority, "");
  BOOST_CHECK_EQUAL(c.path, "example.com");
  BOOST_CHECK_EQUAL(c.query, "");
  BOOST_CHECK_EQUAL(c.fragment, "");
}
