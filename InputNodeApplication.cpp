/**
 * \file InputNodeApplication.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "include.hpp"

#include "global.hpp"

#include "Timeslice.hpp"
#include "Parameters.hpp"
#include "RingBuffer.hpp"
#include "RingBufferView.hpp"
#include "IBConnection.hpp"
#include "IBConnectionGroup.hpp"
#include "InputChannelConnection.hpp"
#include "DataSource.hpp"
#include "FlibPatternGenerator.hpp"
#include "InputChannelSender.hpp"
#include "ComputeNodeConnection.hpp"
#include "concurrent_queue.hpp"
#include "ComputeBuffer.hpp"
#include "Application.hpp"
#include "InputNodeApplication.hpp"

InputNodeApplication::InputNodeApplication(Parameters& par,
                                           std::vector<unsigned> indexes)
    : Application<InputChannelSender>(par),
      _compute_hostnames(par.compute_nodes())
{
    for (unsigned int i = 0; i < par.compute_nodes().size(); ++i)
        _compute_services.push_back(boost::lexical_cast
                                    <std::string>(par.base_port() + i));

    for (unsigned i : indexes) {
        std::unique_ptr<DataSource> data_source(new FlibPatternGenerator(
            par.in_data_buffer_size_exp(), par.in_desc_buffer_size_exp(), i,
            par.check_pattern(), par.typical_content_size(),
            par.randomize_sizes()));
        std::unique_ptr<InputChannelSender> buffer(new InputChannelSender(
            i, *data_source, _compute_hostnames, _compute_services,
            par.timeslice_size(), par.overlap_size(),
            par.max_timeslice_number()));
        _data_sources.push_back(std::move(data_source));
        _buffers.push_back(std::move(buffer));
    }
}
