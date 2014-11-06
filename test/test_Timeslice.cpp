#define BOOST_TEST_MODULE test_Timeslice
#include <boost/test/unit_test.hpp>

#include "StorableTimeslice.hpp"
#include <array>
#include <string>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

struct F {
    F()
    {
        // initialize microslice descriptors (for individual meaning cf.
        // MicrosliceDescriptor.hpp)
        fles::MicrosliceDescriptor desc = fles::MicrosliceDescriptor();
        desc.hdr_id = 0xDD;
        desc.hdr_ver = 0x01;
        desc.sys_id = 0xFF;
        desc.sys_ver = 1;

        desc_a = desc;
        desc_a.eq_id = 10;
        desc_a.idx = 1;
        desc_a.size = data_a.size();

        desc_b = desc;
        desc_b.eq_id = 10;
        desc_b.idx = 2;
        desc_b.size = data_b.size();

        desc_c = desc;
        desc_c.eq_id = 11;
        desc_c.idx = 1;
        desc_c.size = data_c.size();

        // first component: 2 microslices (a, b)
        ts0.append_component(2, 1);
        ts0.append_microslice(0, 0, desc_a, data_a.data());
        ts0.append_microslice(0, 1, desc_b, data_b.data());

        // second component: 1 microslice (c)
        ts0.append_component(1, 1);
        ts0.append_microslice(1, 0, desc_c, data_c.data());
    }

    std::array<uint8_t, 4> data_a{{7, 13, 12, 8}};
    std::array<uint8_t, 1> data_b{{11}};
    std::array<uint8_t, 3> data_c{{3, 4, 5}};

    fles::MicrosliceDescriptor desc_a = fles::MicrosliceDescriptor();
    fles::MicrosliceDescriptor desc_b = fles::MicrosliceDescriptor();
    fles::MicrosliceDescriptor desc_c = fles::MicrosliceDescriptor();

    fles::StorableTimeslice ts0{1, 1};
};

BOOST_AUTO_TEST_CASE(constructor_test)
{
    fles::StorableTimeslice ts1{2};
    BOOST_CHECK_EQUAL(ts1.num_core_microslices(), 2);
}

BOOST_FIXTURE_TEST_CASE(index_test, F)
{
    BOOST_CHECK_EQUAL(ts0.index(), 1);
    BOOST_CHECK_EQUAL(ts0.num_core_microslices(), 1);
    BOOST_CHECK_EQUAL(ts0.num_components(), 2);
    BOOST_CHECK_EQUAL(ts0.num_microslices(0), 2);
    BOOST_CHECK_EQUAL(ts0.num_microslices(1), 1);
}

BOOST_FIXTURE_TEST_CASE(content_access_test, F)
{
    BOOST_CHECK_EQUAL(*ts0.content(0, 1), 11);
    BOOST_CHECK_EQUAL(*ts0.content(1, 0), 3);
}

BOOST_FIXTURE_TEST_CASE(offset_computation_test, F)
{
    fles::StorableTimeslice ts{1};
    ts.append_component(2);
    desc_a.offset = 1234;
    ts.append_microslice(0, 0, desc_a, data_a.data());
    ts.append_microslice(0, 1, desc_b, data_b.data());
    BOOST_CHECK_EQUAL(ts.descriptor(0, 1).offset, 1234 + data_a.size());
}

BOOST_FIXTURE_TEST_CASE(serialization_test, F)
{
    std::stringstream s;
    boost::archive::binary_oarchive oa(s);
    oa << ts0;
    boost::archive::binary_iarchive ia(s);
    fles::StorableTimeslice ts1{0};
    ia >> ts1;
    BOOST_CHECK_EQUAL(ts1.num_core_microslices(), 1);
    BOOST_CHECK_EQUAL(*ts1.content(0, 1), 11);
    BOOST_CHECK_EQUAL(*ts1.content(1, 0), 3);
}
