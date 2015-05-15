// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_Microslice
#include <boost/test/unit_test.hpp>

#include "StorableMicroslice.hpp"
#include "MicrosliceView.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include <array>

struct F {
    F()
    {
        // initialize microslice descriptor (for individual meaning cf.
        // MicrosliceDescriptor.hpp)
        desc0.hdr_id = 0xDD;
        desc0.hdr_ver = 0x01;
        desc0.sys_id = 0xFF;
        desc0.sys_ver = 1;
        desc0.eq_id = 10;
        desc0.idx = 1;
        desc0.size = static_cast<uint32_t>(data0.size());
    }

    const std::array<uint8_t, 4> data0{{7, 13, 12, 8}};

    fles::MicrosliceDescriptor desc0 = fles::MicrosliceDescriptor();
};

BOOST_FIXTURE_TEST_CASE(storable_constructors_test, F)
{
    fles::StorableMicroslice m1(desc0, data0.data());

    std::vector<uint8_t> v{data0.begin(), data0.end()};
    fles::StorableMicroslice m2(desc0, v);

    BOOST_CHECK_EQUAL(m1.desc().eq_id, 10);
    BOOST_CHECK_EQUAL(m2.desc().eq_id, 10);
    BOOST_CHECK_EQUAL(m1.content()[3], 8);
    BOOST_CHECK_EQUAL(m2.content()[3], 8);
}

BOOST_FIXTURE_TEST_CASE(view_constructors_test, F)
{
    fles::MicrosliceView m1(desc0, data0.data());

    fles::StorableMicroslice m2(m1);

    BOOST_CHECK_EQUAL(m1.desc().eq_id, 10);
    BOOST_CHECK_EQUAL(m2.desc().eq_id, 10);
    BOOST_CHECK_EQUAL(m1.content()[3], 8);
    BOOST_CHECK_EQUAL(m2.content()[3], 8);
}

BOOST_FIXTURE_TEST_CASE(storable_assignment_test, F)
{
    fles::StorableMicroslice m1(desc0, data0.data());
    fles::StorableMicroslice m2 = m1;

    BOOST_CHECK_EQUAL(m2.desc().eq_id, 10);
    BOOST_CHECK_EQUAL(m2.content()[3], 8);
}

BOOST_FIXTURE_TEST_CASE(view_assignment_test, F)
{
    fles::MicrosliceView m1(desc0, data0.data());
    fles::MicrosliceView m2 = m1;

    BOOST_CHECK_EQUAL(m2.desc().eq_id, 10);
    BOOST_CHECK_EQUAL(m2.content()[3], 8);
}

BOOST_FIXTURE_TEST_CASE(archive_test, F)
{
    fles::StorableMicroslice m1(desc0, data0.data());
    fles::MicrosliceView m2(desc0, data0.data());

    std::string filename("test1.msa");
    {
        fles::MicrosliceOutputArchive output(filename);
        output.write(m1);
        output.write(m2);
    }
    uint64_t count = 0;
    fles::MicrosliceInputArchive source(filename);
    while (auto microslice = source.get()) {
        BOOST_CHECK_EQUAL(microslice->desc().eq_id, 10);
        BOOST_CHECK_EQUAL(microslice->content()[3], 8);
        ++count;
    }
    BOOST_CHECK_EQUAL(count, 2);
    BOOST_CHECK_EQUAL(source.descriptor().username(),
                      fles::system::current_username());
}

BOOST_AUTO_TEST_CASE(archive_exception_test)
{
    std::string filename("does_not_exist.msa");
    BOOST_CHECK_THROW(fles::MicrosliceInputArchive source(filename),
                      std::ios_base::failure)
}
