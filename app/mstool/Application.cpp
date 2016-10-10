// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "EmbeddedPatternGenerator.hpp"
#include "FlibPatternGenerator.hpp"
#include "MicrosliceAnalyzer.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "MicrosliceReceiver.hpp"
#include "MicrosliceTransmitter.hpp"
#include "TimesliceDebugger.hpp"
#include "log.hpp"
#include "shm_channel_client.hpp"
#include <chrono>
#include <iostream>
#include <thread>


Application::Application(Parameters const& par) : par_(par), etcd(par_.kv_url)
{
    //----------added H.Hartmann 01.09.16----------
    stringstream post;
    
    if(par_.kv_shm == true){
        etcd.checkonprocess(par_.input_shm);
        /*prefix_in << "/" << par_.input_shm;
        ret = etcd.getvalue(prefix_in.str(), "/uptodate");
        if(ret != 0){
            cout << "ret was " << ret << " (1 shm not uptodate, 2 an error occured)" << endl;
            L_(warning) << "no shm set yet";
            ret = etcd.waitvalue(prefix_in.str());
            if(ret != 0){
                cout << "ret was " << ret << " (1 shm not uptodate, 2 an error occured)" << endl;
                L_(warning) << "no shm set";
                exit (EXIT_FAILURE);
            }
        }
        cout << "setting " << par_.input_shm << " value to off" << endl;
        etcd.setvalue(prefix_in.str(), "/uptodate", "value=off");
        L_(info) << "flag for shm was set in kv-store";*/
    }

    // Source setup
    if (!par_.input_shm.empty()) {
        //get it from etcd
        L_(info) << "using shared memory as data source: " << par_.input_shm;

        shm_device_ = std::make_shared<flib_shm_device_client>(par_.input_shm);

        if (par_.shm_channel < shm_device_->num_channels()) {
            data_source_.reset(
                new flib_shm_channel_client(shm_device_, par_.shm_channel));

        } else {
            throw std::runtime_error("shared memory channel not available");
        }
    } else if (par_.use_pattern_generator) {
        L_(info) << "using pattern generator as data source";

        constexpr uint32_t typical_content_size = 10000;
        constexpr std::size_t desc_buffer_size_exp = 19; // 512 ki entries
        constexpr std::size_t data_buffer_size_exp = 27; // 128 MiB

        switch (par_.pattern_generator) {
        case 1:
            data_source_.reset(new FlibPatternGenerator(
                data_buffer_size_exp, desc_buffer_size_exp, 0,
                typical_content_size, true, true));
            break;
        case 2:
            data_source_.reset(new EmbeddedPatternGenerator(
                data_buffer_size_exp, desc_buffer_size_exp, 0,
                typical_content_size, true, true));
            break;
        default:
            throw std::runtime_error("pattern generator type not available");
        }
    }

    if (data_source_) {
        source_.reset(new fles::MicrosliceReceiver(*data_source_));
    } else if (!par_.input_archive.empty()) {
        source_.reset(new fles::MicrosliceInputArchive(par_.input_archive));
    }

    // Sink setup
    if (par_.analyze) {
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new MicrosliceAnalyzer(10000, std::cout, "")));
    }

    if (par_.dump_verbosity > 0) {
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new MicrosliceDumper(std::cout, par_.dump_verbosity)));
    }

    if (!par_.output_archive.empty()) {
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new fles::MicrosliceOutputArchive(par_.output_archive)));
    }

    if (!par_.output_shm.empty()) {
        L_(info) << "providing output in shared memory: " << par_.output_shm;

        constexpr std::size_t desc_buffer_size_exp = 19; // 512 ki entries
        constexpr std::size_t data_buffer_size_exp = 27; // 128 MiB

        output_shm_device_.reset(new flib_shm_device_provider(
            par_.output_shm, 1, data_buffer_size_exp, desc_buffer_size_exp));
        InputBufferWriteInterface* data_sink =
            output_shm_device_->channels().at(0);
        sinks_.push_back(std::unique_ptr<fles::MicrosliceSink>(
            new fles::MicrosliceTransmitter(*data_sink)));
        
        //----------added H.Hartmann 01.09.16----------
        string post = "value=on";
        prefix_out << "/" << par_.output_shm;
        etcd.setvalue(prefix_out.str(), "/uptodate", post);
    }
}

Application::~Application()
{
    etcd.setvalue(prefix_out.str(), "/uptodate", "value=off");
    L_(info) << "total microslices processed: " << count_;
}

void Application::run()
{
    uint64_t limit = par_.maximum_number;

    while (auto microslice = source_->get()) {
        std::shared_ptr<const fles::Microslice> ms(std::move(microslice));
        for (auto& sink : sinks_) {
            sink->put(ms);
        }
        ++count_;
        if (count_ == limit) {
            break;
        }
    }
    for (auto& sink : sinks_) {
        sink->end_stream();
    }
    if (output_shm_device_) {
        L_(info) << "waiting until output shared memory is empty";
        while (!output_shm_device_->channels().at(0)->empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
