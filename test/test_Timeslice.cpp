// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_Timeslice
#include <boost/test/unit_test.hpp>

#include "StorableTimeslice.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceOutputArchive.hpp"
#include "MicrosliceView.hpp"
#include "System.hpp"
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
        desc.hdr_id =
            static_cast<uint8_t>(fles::HeaderFormatIdentifier::Standard);
        desc.hdr_ver =
            static_cast<uint8_t>(fles::HeaderFormatVersion::Standard);
        desc.sys_id = static_cast<uint8_t>(fles::SubsystemIdentifier::FLES);
        desc.sys_ver =
            static_cast<uint8_t>(fles::SubsystemFormatFLES::Uninitialized);

        desc_a = desc;
        desc_a.eq_id = 10;
        desc_a.idx = 1;
        desc_a.size = static_cast<uint32_t>(data_a.size());

        desc_b = desc;
        desc_b.eq_id = 10;
        desc_b.idx = 2;
        desc_b.size = static_cast<uint32_t>(data_b.size());

        desc_c = desc;
        desc_c.eq_id = 11;
        desc_c.idx = 1;
        desc_c.size = static_cast<uint32_t>(data_c.size());

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

BOOST_FIXTURE_TEST_CASE(archive_test, F)
{
    auto ts0_ptr = std::make_shared<const fles::StorableTimeslice>(ts0);

    std::string filename("test1.tsa");
    {
        fles::TimesliceOutputArchive output(filename);
        output.put(ts0_ptr);
        output.put(ts0_ptr);
    }
    uint64_t count = 0;
    fles::TimesliceInputArchive source(filename);
    while (auto timeslice = source.get()) {
        BOOST_CHECK_EQUAL(timeslice->num_core_microslices(), 1);
        BOOST_CHECK_EQUAL(*timeslice->content(0, 1), 11);
        BOOST_CHECK_EQUAL(*timeslice->content(1, 0), 3);
        ++count;
    }
    BOOST_CHECK_EQUAL(count, 2);
    BOOST_CHECK_EQUAL(source.descriptor().username(),
                      fles::system::current_username());
}

BOOST_AUTO_TEST_CASE(archive_exception_test)
{
    std::string filename("does_not_exist.tsa");
    BOOST_CHECK_THROW(fles::TimesliceInputArchive source(filename),
                      std::ios_base::failure);
}

BOOST_FIXTURE_TEST_CASE(microslice_access_test, F)
{
    fles::MicrosliceView m = ts0.get_microslice(1, 0);
    const uint8_t* content = m.content();
    BOOST_CHECK_EQUAL(content[2], 5);
    BOOST_CHECK_EQUAL(m.desc().eq_id, 11);
}

BOOST_FIXTURE_TEST_CASE(microslice_storage_test, F)
{
    fles::MicrosliceView mv = ts0.get_microslice(0, 1);
    uint32_t c = ts0.append_component(1);
    fles::StorableMicroslice m = mv;
    ts0.append_microslice(c, 0, m);
    BOOST_CHECK_EQUAL(ts0.descriptor(c, 0).size, 1);
}

BOOST_AUTO_TEST_CASE(reference_file_existence_test)
{
    std::string filename("example1.tsa");
    BOOST_CHECK_NO_THROW(fles::TimesliceInputArchive source(filename));
}

BOOST_AUTO_TEST_CASE(reference_archive_test)
{
    std::string filename("example1.tsa");
    uint64_t count = 0;
    fles::TimesliceInputArchive source(filename);
    while (auto timeslice = source.get()) {
        BOOST_CHECK_EQUAL(timeslice->index(), 1);
        BOOST_CHECK_EQUAL(timeslice->num_core_microslices(), 1);
        BOOST_CHECK_EQUAL(timeslice->num_components(), 2);
        BOOST_CHECK_EQUAL(timeslice->num_microslices(0), 2);
        BOOST_CHECK_EQUAL(timeslice->num_microslices(1), 1);
        BOOST_CHECK_EQUAL(*timeslice->content(0, 1), 11);
        BOOST_CHECK_EQUAL(*timeslice->content(1, 0), 3);
        ++count;
    }
    BOOST_CHECK_EQUAL(count, 2);
    BOOST_CHECK_EQUAL(source.descriptor().username(), "jan");
    BOOST_CHECK_EQUAL(source.descriptor().hostname(), "ten-3.fritz.box");
}

BOOST_AUTO_TEST_CASE(invalid_archive_test)
{
    std::string filename1("test_Timeslice");
    // BOOST_CHECK_THROW(fles::TimesliceInputArchive source(filename1),
    //                   boost::archive::archive_exception);

    std::string filename2("example1.msa");
    BOOST_CHECK_THROW(fles::TimesliceInputArchive source(filename2),
                      std::runtime_error);
}
