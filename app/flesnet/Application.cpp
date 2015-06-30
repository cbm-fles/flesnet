// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "FlibHardwareChannel.hpp"
#include "FlibShmChannel.hpp"
#include "FlibPatternGenerator.hpp"
#include "EmbeddedPatternGenerator.hpp"
#include <log.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/future.hpp>

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : _par(par), _signal_status(signal_status)
{
    unsigned input_nodes_size = par.input_nodes().size();
    std::vector<unsigned> input_indexes = par.input_indexes();

    // FIXME: some of this is a terrible mess
    if (par.use_flib()) {
        if (par.use_shared_memory()) {
            try {
                _shm_device =
                    std::unique_ptr<shm_device_client>(new shm_device_client());
                _shm_num_channels = _shm_device->num_channels();
                L_(info) << "using shared memory";

                // increase number of input nodes to match number of
                // enabled FLIB links if in stand-alone mode
                if (par.standalone() && _shm_num_channels > 1) {
                    input_nodes_size = _shm_num_channels;
                    for (unsigned i = 1; i < input_nodes_size; i++) {
                        input_indexes.push_back(i);
                    }
                }

            } catch (std::exception const& e) {
                L_(error) << "exception while creating flib: " << e.what();
            }
        } else {
            // TODO: presence detection #524
            try {
                _flib = std::unique_ptr<flib::flib_device>(
                    new flib::flib_device(0));
                _flib_links = _flib->links();

                // delete deactivated links from vector
                _flib_links.erase(
                    std::remove_if(std::begin(_flib_links),
                                   std::end(_flib_links),
                                   [](decltype(_flib_links[0]) link) {
                                       return link->data_sel() ==
                                              flib::flib_link::rx_disable;
                                   }),
                    std::end(_flib_links));

                L_(info) << "enabled flib links detected: "
                         << _flib_links.size();

                // increase number of input nodes to match number of
                // enabled FLIB links if in stand-alone mode
                if (par.standalone() && _flib_links.size() > 1) {
                    input_nodes_size = _flib_links.size();
                    for (unsigned i = 1; i < input_nodes_size; i++) {
                        input_indexes.push_back(i);
                    }
                }
            } catch (std::exception const& e) {
                L_(error) << "exception while creating flib: " << e.what();
            }
        }
    }
    // end FIXME

    if (par.standalone()) {
        L_(info) << "flesnet in stand-alone mode, inputs: " << input_nodes_size;
    }

    // Compute node application

    // set_cpu(1);

    for (unsigned i : _par.compute_indexes()) {
        std::unique_ptr<ComputeBuffer> buffer(new ComputeBuffer(
            i, _par.cn_data_buffer_size_exp(), _par.cn_desc_buffer_size_exp(),
            _par.base_port() + i, input_nodes_size, _par.timeslice_size(),
            _par.processor_instances(), _par.processor_executable(),
            _signal_status));
        buffer->start_processes();
        _compute_buffers.push_back(std::move(buffer));
    }

    set_node();

    // Input node application

    std::vector<std::string> compute_services;
    for (unsigned int i = 0; i < par.compute_nodes().size(); ++i)
        compute_services.push_back(
            boost::lexical_cast<std::string>(par.base_port() + i));

    for (size_t c = 0; c < input_indexes.size(); ++c) {
        unsigned index = input_indexes.at(c);
        std::unique_ptr<DataSource> data_source;

        if (c < _flib_links.size()) {
            data_source = std::unique_ptr<DataSource>(new FlibHardwareChannel(
                par.in_data_buffer_size_exp(), par.in_desc_buffer_size_exp(),
                _flib_links.at(c)));
        } else if (c < _shm_num_channels) {
            data_source = std::unique_ptr<DataSource>(
                new FlibShmChannel(_shm_device->channels().at(c)));
        } else {
            if (false) {
                data_source =
                    std::unique_ptr<DataSource>(new FlibPatternGenerator(
                        par.in_data_buffer_size_exp(),
                        par.in_desc_buffer_size_exp(), index,
                        par.typical_content_size()));
            } else {
                data_source =
                    std::unique_ptr<DataSource>(new EmbeddedPatternGenerator(
                        par.in_data_buffer_size_exp(),
                        par.in_desc_buffer_size_exp(), index,
                        par.typical_content_size()));
            }
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
    try {
        if (_flib) {
            _flib->enable_mc_cnt(false);
        }
    } catch (std::exception& e) {
        L_(error) << "exception in destructor ~InputNodeApplication(): "
                  << e.what();
    }
}

void Application::run()
{
    // Do not spawn additional thread if only one is needed, simplifies
    // debugging
    if (_compute_buffers.size() == 1 && _input_channel_senders.empty()) {
        L_(debug) << "using existing thread for single compute buffer";
        (*_compute_buffers[0])();
        return;
    };
    if (_input_channel_senders.size() == 1 && _compute_buffers.empty()) {
        L_(debug) << "using existing thread for single input buffer";
        (*_input_channel_senders[0])();
        return;
    };

    // FIXME: temporary code, need to implement interrupt
    boost::thread_group threads;
    std::vector<boost::unique_future<void>> futures;
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

    L_(debug) << "threads started: " << threads.size();

    while (!futures.empty()) {
        auto it = boost::wait_for_any(futures.begin(), futures.end());
        try {
            it->get();
        } catch (const std::exception& e) {
            L_(fatal) << "exception from thread: " << e.what();
            stop = true;
        }
        futures.erase(it);
        if (stop)
            threads.interrupt_all();
    }

    threads.join_all();
}
