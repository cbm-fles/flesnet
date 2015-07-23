// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_MicrosliceReceiver
#include <boost/test/unit_test.hpp>

#include "FlibPatternGenerator.hpp"
#include "EmbeddedPatternGenerator.hpp"
#include "MicrosliceReceiver.hpp"
#include "MicrosliceOutputArchive.hpp"
#include <iostream>

BOOST_AUTO_TEST_CASE(usage_test)
{
    uint32_t typical_content_size = 10240;
    std::size_t desc_buffer_size_exp = 7;  // 128 entries
    std::size_t data_buffer_size_exp = 20; // 1 MiB

    std::unique_ptr<DataSource> data_source0(new FlibPatternGenerator(
        data_buffer_size_exp, desc_buffer_size_exp, 0, typical_content_size));

    std::unique_ptr<DataSource> data_source1(new EmbeddedPatternGenerator(
        data_buffer_size_exp, desc_buffer_size_exp, 1, typical_content_size));

    std::unique_ptr<fles::MicrosliceSource> ms0(
        new fles::MicrosliceReceiver(*data_source0));

    std::unique_ptr<fles::MicrosliceSource> ms1(
        new fles::MicrosliceReceiver(*data_source1));

    std::unique_ptr<fles::MicrosliceOutputArchive> output(
        new fles::MicrosliceOutputArchive("output.msa"));

    std::size_t count = 0;
    while (auto microslice = ms0->get()) {
        std::cout << microslice->desc().size << std::endl;
        output->write(*microslice);
        ++count;
        if (count == 10) {
            break;
        }
    }

    BOOST_CHECK_EQUAL(count, 10);
}
