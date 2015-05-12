#define BOOST_TEST_MODULE test_Microslice
#include <boost/test/unit_test.hpp>

struct F {
    F() { i = 1; }

    int i;
};

BOOST_FIXTURE_TEST_CASE(simple, F) { BOOST_CHECK_EQUAL(i, 1); }
