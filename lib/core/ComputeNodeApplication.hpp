#pragma once
/**
 * \file ComputeNodeApplication.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "Application.hpp"
#include "ComputeBuffer.hpp"

/// Compute application class.
/** The ComputeNodeApplication object represents an instance of the
    running compute node application. */

class ComputeNodeApplication : public Application<ComputeBuffer>
{
public:
    /// The ComputeNodeApplication contructor.
    ComputeNodeApplication(Parameters& par, std::vector<unsigned> indexes);

    ComputeNodeApplication(const ComputeNodeApplication&) = delete;
    void operator=(const ComputeNodeApplication&) = delete;

private:
    static void child_handler(int sig);
};
