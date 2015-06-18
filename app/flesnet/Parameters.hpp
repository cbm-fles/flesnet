// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <stdexcept>
#include <vector>

/// Run parameter exception class.
/** A ParametersException object signals an error in a given parameter
    on the command line or in a configuration file. */

class ParametersException : public std::runtime_error
{
public:
    /// The ParametersException constructor.
    explicit ParametersException(const std::string& what_arg = "")
        : std::runtime_error(what_arg)
    {
    }
};

/// Global run parameter class.
/** A Parameters object stores the information given on the command
    line or in a configuration file. */

class Parameters
{
public:
    /// The Parameters command-line parsing constructor.
    Parameters(int argc, char* argv[]) { parse_options(argc, argv); }

    Parameters(const Parameters&) = delete;
    void operator=(const Parameters&) = delete;

    /// Return a description of active nodes, suitable for debug output.
    std::string const desc() const;

    uint32_t suggest_in_data_buffer_size_exp();
    uint32_t suggest_cn_data_buffer_size_exp();

    uint32_t suggest_in_desc_buffer_size_exp();
    uint32_t suggest_cn_desc_buffer_size_exp();

    /// Retrieve the global timeslice size in number of MCs.
    uint32_t timeslice_size() const { return _timeslice_size; }

    /// Retrieve the size of the overlap region in number of MCs.
    uint32_t overlap_size() const { return _overlap_size; }

    /// Retrieve the exp. size of the input node's data buffer in bytes.
    uint32_t in_data_buffer_size_exp() const
    {
        return _in_data_buffer_size_exp;
    }

    /// Retrieve the exp. size of the input node's descriptor buffer
    /// (number of entries).
    uint32_t in_desc_buffer_size_exp() const
    {
        return _in_desc_buffer_size_exp;
    }

    /// Retrieve the exp. size of the compute node's data buffer in bytes.
    uint32_t cn_data_buffer_size_exp() const
    {
        return _cn_data_buffer_size_exp;
    }

    /// Retrieve the exp. size of the compute node's descriptor buffer
    /// (number of entries).
    uint32_t cn_desc_buffer_size_exp() const
    {
        return _cn_desc_buffer_size_exp;
    }

    /// Retrieve the typical number of content bytes per MC.
    uint32_t typical_content_size() const { return _typical_content_size; }

    /// Retrieve the use flib flag.
    bool use_flib() const { return _use_flib; }

    /// Retrieve the use shared memory flag.
    bool use_shared_memory() const { return _use_shared_memory; }

    /// Retrieve the standalone mode flag.
    bool standalone() const { return _standalone; }

    /// Retrieve the global maximum timeslice number.
    uint32_t max_timeslice_number() const { return _max_timeslice_number; }

    /// Retrieve the name of the executable acting as timeslice processor.
    std::string processor_executable() const { return _processor_executable; }

    /// Retrieve the number of instances of the timeslice processor executable.
    uint32_t processor_instances() const { return _processor_instances; }

    /// Retrieve the global base port.
    uint32_t base_port() const { return _base_port; }

    /// Retrieve the number of completion queue entries.
    uint32_t num_cqe() const { return _num_cqe; }

    /// Retrieve the list of participating input nodes.
    std::vector<std::string> const input_nodes() const { return _input_nodes; }

    /// Retrieve the list of participating compute nodes.
    std::vector<std::string> const compute_nodes() const
    {
        return _compute_nodes;
    }

    /// Retrieve this applications's indexes in the list of input nodes.
    std::vector<unsigned> input_indexes() const { return _input_indexes; }

    /// Retrieve this applications's indexes in the list of compute nodes.
    std::vector<unsigned> compute_indexes() const { return _compute_indexes; }

private:
    /// Parse command line options.
    void parse_options(int argc, char* argv[]);

    /// Print buffer information
    void print_buffer_info();

    /// The global timeslice size in number of MCs.
    uint32_t _timeslice_size = 100;

    /// The size of the overlap region in number of MCs.
    uint32_t _overlap_size = 2;

    /// A typical number of content bytes per MC.
    uint32_t _typical_content_size = 1024;

    /// The exp. size of the input node's data buffer in bytes.
    uint32_t _in_data_buffer_size_exp = 0;

    /// The exp. size of the input node's descriptor buffer (number of
    /// entries).
    uint32_t _in_desc_buffer_size_exp = 0;

    /// The exp. size of the compute node's data buffer in bytes.
    uint32_t _cn_data_buffer_size_exp = 0;

    /// The exp. size of the compute node's descriptor buffer (number of
    /// entries).
    uint32_t _cn_desc_buffer_size_exp = 0;

    /// The use flib flag.
    bool _use_flib = true;

    /// The use shared memory flag.
    bool _use_shared_memory = false;

    /// The standalone mode flag.
    bool _standalone = true;

    /// The global maximum timeslice number.
    uint32_t _max_timeslice_number = UINT32_MAX;

    /// The name of the executable acting as timeslice processor.
    std::string _processor_executable;

    /// The number of instances of the timeslice processor executable.
    uint32_t _processor_instances = 2;

    /// The global base port.
    uint32_t _base_port = 20079;

    uint32_t _num_cqe = 1000000;

    /// The list of participating input nodes.
    std::vector<std::string> _input_nodes;

    /// The list of participating compute nodes.
    std::vector<std::string> _compute_nodes;

    /// This applications's indexes in the list of input nodes.
    std::vector<unsigned> _input_indexes;

    /// This applications's indexes in the list of compute nodes.
    std::vector<unsigned> _compute_indexes;
};
