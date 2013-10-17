/**
 * \file InputNodeApplication.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "InputNodeApplication.hpp"
#include "FlibHardwareChannel.hpp"
#include "FlibPatternGenerator.hpp"
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <thread>

InputNodeApplication::InputNodeApplication(Parameters& par,
                                           std::vector<unsigned> indexes)
    : Application<InputChannelSender>(par),
      _compute_hostnames(par.compute_nodes())
{
    for (unsigned int i = 0; i < par.compute_nodes().size(); ++i)
        _compute_services.push_back(boost::lexical_cast
                                    <std::string>(par.base_port() + i));

    // FIXME: all of this is a terrible mess
    if (par.use_flib()) {
        try
        {
            _flib = std::unique_ptr
                <flib::flib_device>(new flib::flib_device(0));
        }
        catch (std::exception const& e)
        {
        }

        if (_flib) {
            _flib_links = _flib->get_links();
            if (_flib_links.size() == 255) {
                _flib = nullptr;
                _flib_links.clear();
            }
        }

        if (_flib_links.empty())
            out.warn() << "no flib links detected";
        else
            out.info() << "flib hardware links: " << _flib_links.size();

        if (_flib) {
            _flib->enable_mc_cnt(false);
            for (auto fl : _flib_links) {
                fl->enable_cbmnet_packer(false);
                fl->rst_pending_mc();
                fl->set_start_idx(0);
                fl->init_dma(flib::open_or_create, 20, 15);
            }
            _flib->enable_mc_cnt(true);
            std::chrono::microseconds interval1(10);
            std::this_thread::sleep_for(interval1);
            _flib->enable_mc_cnt(false);

            std::chrono::milliseconds interval2(100);
            std::this_thread::sleep_for(interval2);
            _flib = nullptr;
            _flib_links.clear();

            _flib = std::unique_ptr
                <flib::flib_device>(new flib::flib_device(0));
            _flib_links = _flib->get_links();
        }
    }

    for (size_t c = 0; c < indexes.size(); ++c) {
        unsigned index = indexes.at(c);
        std::unique_ptr<DataSource> data_source;

        if (c < _flib_links.size()) {
            data_source = std::unique_ptr<DataSource>(new FlibHardwareChannel(
                par.in_data_buffer_size_exp(), par.in_desc_buffer_size_exp(),
                index, _flib_links.at(c)));
        } else {
            data_source = std::unique_ptr<DataSource>(new FlibPatternGenerator(
                par.in_data_buffer_size_exp(), par.in_desc_buffer_size_exp(),
                index, par.check_pattern(), par.typical_content_size(),
                par.randomize_sizes()));
        }

        std::unique_ptr<InputChannelSender> buffer(new InputChannelSender(
            index, *data_source, _compute_hostnames, _compute_services,
            par.timeslice_size(), par.overlap_size(),
            par.max_timeslice_number()));

        _data_sources.push_back(std::move(data_source));
        _buffers.push_back(std::move(buffer));
    }

    if (_flib) {
        _flib->enable_mc_cnt(true);
    }
}

InputNodeApplication::~InputNodeApplication()
{
    if (_flib) {
        _flib->enable_mc_cnt(false);
    }
}
