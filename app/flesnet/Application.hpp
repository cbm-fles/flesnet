#pragma once
/**
 * \file Application.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "ThreadContainer.hpp"
#include "Parameters.hpp"
#include "ComputeBuffer.hpp"
#include "InputChannelSender.hpp"
#include <flib.h>
#include <boost/lexical_cast.hpp>
#include <memory>

/// %Application base class.
/** The Application object represents an instance of the running
    application. */

class Application : public ThreadContainer
{
public:
    /// The Application contructor.
    explicit Application(Parameters const& par);

    /// The Application destructor.
    ~Application();

    void run();

    Application(const Application&) = delete;
    void operator=(const Application&) = delete;

private:
    /// The run parameters object.
    Parameters const& _par;

    /// The application's connection group / buffer objects
    std::vector<std::unique_ptr<ComputeBuffer> > _compute_buffers;
    std::vector<std::unique_ptr<InputChannelSender> > _input_channel_senders;

    // Input node application
    std::unique_ptr<flib::flib_device> _flib;
    std::vector<flib::flib_link*> _flib_links;

    std::vector<std::unique_ptr<DataSource> > _data_sources;
};
