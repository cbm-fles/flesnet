/**
 * \file InputNodeApplication.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef INPUTNODEAPPLICATION_HPP
#define INPUTNODEAPPLICATION_HPP

/// Input application class.
/** The InputNodeApplication object represents an instance of the running
    input node application. */

class InputNodeApplication : public Application<InputChannelSender>
{
public:

    /// The InputNodeApplication contructor.
    explicit InputNodeApplication(Parameters& par, std::vector<unsigned> indexes) :
        Application<InputChannelSender>(par),
        _compute_hostnames(par.compute_nodes())
    {
        for (unsigned int i = 0; i < par.compute_nodes().size(); i++)
            _compute_services.push_back(boost::lexical_cast<std::string>(par.base_port() + i));

        for (unsigned i: indexes) {
            std::unique_ptr<DataSource> data_source
                (new DummyFlib(par.in_data_buffer_size_exp(), par.in_desc_buffer_size_exp(),
                               i, par.check_pattern(), par.typical_content_size(),
                               par.randomize_sizes()));
            std::unique_ptr<InputChannelSender> buffer
                (new InputChannelSender(i, *data_source, _compute_hostnames, _compute_services,
                                 par.timeslice_size(), par.overlap_size(),
                                 par.max_timeslice_number()));
            _data_sources.push_back(std::move(data_source));
            _buffers.push_back(std::move(buffer));
        }
    }

private:
    std::vector<std::string> _compute_hostnames;
    std::vector<std::string> _compute_services;

    std::vector<std::unique_ptr<DataSource> > _data_sources;
};


#endif /* INPUTNODEAPPLICATION_HPP */
