/**
 * \file Application.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "Application.hpp"
#include "FlibHardwareChannel.hpp"
#include "FlibPatternGenerator.hpp"
#include <boost/thread/thread.hpp>
#include <boost/thread/future.hpp>

Application::Application(Parameters const& par) : _par(par)
{
    // Compute node application

    // set_cpu(1);

    for (unsigned i : _par.compute_indexes()) {
        std::unique_ptr<ComputeBuffer> buffer(
            new ComputeBuffer(i,
                              _par.cn_data_buffer_size_exp(),
                              _par.cn_desc_buffer_size_exp(),
                              _par.base_port() + i,
                              _par.input_nodes().size(),
                              _par.timeslice_size(),
                              _par.overlap_size(),
                              _par.processor_instances(),
                              _par.processor_executable()));
        buffer->start_processes();
        _compute_buffers.push_back(std::move(buffer));
    }

    set_node();

    // Input node application

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
    // end FIXME

    std::vector<std::string> compute_services;
    for (unsigned int i = 0; i < par.compute_nodes().size(); ++i)
        compute_services.push_back(boost::lexical_cast
                                   <std::string>(par.base_port() + i));

    for (size_t c = 0; c < _par.input_indexes().size(); ++c) {
        unsigned index = _par.input_indexes().at(c);
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
            index, *data_source, par.compute_nodes(), compute_services,
            par.timeslice_size(), par.overlap_size(),
            par.max_timeslice_number()));

        _data_sources.push_back(std::move(data_source));
        _input_channel_senders.push_back(std::move(buffer));
    }

    if (_flib) {
        _flib->enable_mc_cnt(true);
    }
}

Application::~Application()
{
    // Input node application
    try
    {
        if (_flib) {
            _flib->enable_mc_cnt(false);
        }
    }
    catch (std::exception& e)
    {
        out.error() << "exception in destructor ~InputNodeApplication(): "
                    << e.what();
    }
}

void Application::run()
{
    // FIXME: temporary code, need to implement interrupt
    boost::thread_group threads;
    std::vector<boost::unique_future<void> > futures;
    bool stop = false;

    for (auto& buffer : _compute_buffers) {
        boost::packaged_task<void> task(std::ref(*buffer));
        futures.push_back(task.get_future());
        threads.add_thread(new boost::thread(std::move(task)));
    }
    for (auto& buffer : _input_channel_senders) {
        boost::packaged_task<void> task(std::ref(*buffer));
        futures.push_back(task.get_future());
        threads.add_thread(new boost::thread(std::move(task)));
    }

    out.debug() << "threads started: " << threads.size();

    while (!futures.empty()) {
        auto it = boost::wait_for_any(futures.begin(), futures.end());
        try
        {
            it->get();
        }
        catch (const std::exception& e)
        {
            out.fatal() << "exception from thread: " << e.what();
            stop = true;
        }
        futures.erase(it);
        if (stop)
            threads.interrupt_all();
    }

    threads.join_all();
}
