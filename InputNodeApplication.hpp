#pragma once
/**
 * \file InputNodeApplication.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "Application.hpp"
#include "InputChannelSender.hpp"

/// Input application class.
/** The InputNodeApplication object represents an instance of the running
    input node application. */

class InputNodeApplication : public Application<InputChannelSender>
{
public:
    /// The InputNodeApplication contructor.
    InputNodeApplication(Parameters& par, std::vector<unsigned> indexes);

    InputNodeApplication(const InputNodeApplication&) = delete;
    void operator=(const InputNodeApplication&) = delete;

private:
    std::vector<std::string> _compute_hostnames;
    std::vector<std::string> _compute_services;

    std::vector<std::unique_ptr<DataSource> > _data_sources;
};
