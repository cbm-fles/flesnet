// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <stdexcept>
#include <string>
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

    /// Retrieve the global timeslice size in number of microslices.
    uint32_t timeslice_size() const { return timeslice_size_; }

    /// Retrieve the size of the overlap region in number of microslices.
    uint32_t overlap_size() const { return overlap_size_; }

    /// Retrieve the exp. size of the input node's data buffer in bytes.
    uint32_t in_data_buffer_size_exp() const
    {
        return in_data_buffer_size_exp_;
    }

    /// Retrieve the exp. size of the input node's descriptor buffer
    /// (number of entries).
    uint32_t in_desc_buffer_size_exp() const
    {
        return in_desc_buffer_size_exp_;
    }

    /// Retrieve the exp. size of the compute node's data buffer in bytes.
    uint32_t cn_data_buffer_size_exp() const
    {
        return cn_data_buffer_size_exp_;
    }

    /// Retrieve the exp. size of the compute node's descriptor buffer
    /// (number of entries).
    uint32_t cn_desc_buffer_size_exp() const
    {
        return cn_desc_buffer_size_exp_;
    }

    /// Retrieve the typical number of content bytes per microslice.
    uint32_t typical_content_size() const { return typical_content_size_; }

    /// Retrieve the shared memory identifier.
    std::string input_shm() const { return input_shm_; }

    /// Retrieve the standalone mode flag.
    bool standalone() const { return standalone_; }

    /// Retrieve the global maximum timeslice number.
    uint32_t max_timeslice_number() const { return max_timeslice_number_; }

    /// Retrieve the name of the executable acting as timeslice processor.
    std::string processor_executable() const { return processor_executable_; }

    /// Retrieve the number of instances of the timeslice processor executable.
    uint32_t processor_instances() const { return processor_instances_; }

    /// Retrieve the global base port.
    uint32_t base_port() const { return base_port_; }

    /// Retrieve the zeromq transport usage flag
    bool zeromq() const { return zeromq_; }

    /// Generate patterns while generating embedded timeslices
    bool generate_ts_patterns() const { return generate_ts_patterns_; }

    /// Generate timeslices with random sizes
    bool random_ts_sizes() const { return random_ts_sizes_; }

    /// flag to check whether libfabric implementation will be used
    bool use_libfabric() const { return use_libfabric_; }

    /// Retrieve the number of completion queue entries.
    uint32_t num_cqe() const { return num_cqe_; }

    /// Retrieve the list of participating input nodes.
    std::vector<std::string> const input_nodes() const { return input_nodes_; }

    /// Retrieve the list of participating compute nodes.
    std::vector<std::string> const compute_nodes() const
    {
        return compute_nodes_;
    }

    /// Retrieve this applications's indexes in the list of input nodes.
    std::vector<unsigned> input_indexes() const { return input_indexes_; }

    /// Retrieve this applications's indexes in the list of compute nodes.
    std::vector<unsigned> compute_indexes() const { return compute_indexes_; }

private:
    /// Parse command line options.
    void parse_options(int argc, char* argv[]);

    /// Print buffer information
    void print_buffer_info();

    /// The global timeslice size in number of microslices.
    uint32_t timeslice_size_ = 100;

    /// The size of the overlap region in number of microslices.
    uint32_t overlap_size_ = 2;

    /// A typical number of content bytes per microslice.
    uint32_t typical_content_size_ = 1024;

    /// The exp. size of the input node's data buffer in bytes.
    uint32_t in_data_buffer_size_exp_ = 0;

    /// The exp. size of the input node's descriptor buffer (number of
    /// entries).
    uint32_t in_desc_buffer_size_exp_ = 0;

    /// The exp. size of the compute node's data buffer in bytes.
    uint32_t cn_data_buffer_size_exp_ = 0;

    /// The exp. size of the compute node's descriptor buffer (number of
    /// entries).
    uint32_t cn_desc_buffer_size_exp_ = 0;

    // The input shared memory identifier
    std::string input_shm_;

    /// The standalone mode flag.
    bool standalone_ = true;

    /// The global maximum timeslice number.
    uint32_t max_timeslice_number_ = UINT32_MAX;

    /// The name of the executable acting as timeslice processor.
    std::string processor_executable_;

    /// The number of instances of the timeslice processor executable.
    uint32_t processor_instances_ = 2;

    /// The global base port.
    uint32_t base_port_ = 20079;

    /// Retrieve the zeromq transport usage flag
    bool zeromq_ = false;

    /// Generate patterns while generating embedded timeslices
    bool generate_ts_patterns_ = false;

    /// Generate timeslices with random sizes
    bool random_ts_sizes_ = false;

    /// flag to check whether libfabric implementation will be used
    bool use_libfabric_ = false;

    uint32_t num_cqe_ = 1000000;

    /// The list of participating input nodes.
    std::vector<std::string> input_nodes_;

    /// The list of participating compute nodes.
    std::vector<std::string> compute_nodes_;

    /// This applications's indexes in the list of input nodes.
    std::vector<unsigned> input_indexes_;

    /// This applications's indexes in the list of compute nodes.
    std::vector<unsigned> compute_indexes_;
};
