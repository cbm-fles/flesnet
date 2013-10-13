#pragma once
/**
 * \file InputNodeApplication.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "Application.hpp"
#include "InputChannelSender.hpp"
#include <flib.h>

/// Input application class.
/** The InputNodeApplication object represents an instance of the running
    input node application. */

class InputNodeApplication : public Application<InputChannelSender>
{
public:
    /// The InputNodeApplication contructor.
    InputNodeApplication(Parameters& par, std::vector<unsigned> indexes);

    ~InputNodeApplication();

    InputNodeApplication(const InputNodeApplication&) = delete;
    void operator=(const InputNodeApplication&) = delete;

private:
    std::vector<std::string> _compute_hostnames;
    std::vector<std::string> _compute_services;

    std::unique_ptr<flib::flib_device> _flib;
    std::vector<flib::flib_link*> _flib_links;

    std::vector<std::unique_ptr<DataSource> > _data_sources;
};
