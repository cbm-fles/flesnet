// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <map>
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

struct InterfaceSpecification {
    std::string full_uri;
    std::string scheme;
    std::string host;
    std::vector<std::string> path;
    std::map<std::string, std::string> param;
};

/// Transport implementation enum.
enum class Transport { RDMA, LibFabric, ZeroMQ };

std::istream& operator>>(std::istream& in, Transport& transport);
std::ostream& operator<<(std::ostream& out, const Transport& transport);

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

    /// Retrieve the global timeslice size in number of microslices.
    uint32_t timeslice_size() const { return timeslice_size_; }

    /// Retrieve the global maximum timeslice number.
    uint32_t max_timeslice_number() const { return max_timeslice_number_; }

    /// Retrieve the name of the executable acting as timeslice processor.
    std::string processor_executable() const { return processor_executable_; }

    /// Retrieve the number of instances of the timeslice processor executable.
    uint32_t processor_instances() const { return processor_instances_; }

    /// Retrieve the global base port.
    uint32_t base_port() const { return base_port_; }

    /// Retrieve the selected transport implementation.
    Transport transport() const { return transport_; }

    /// Generate patterns while generating embedded timeslices
    bool generate_ts_patterns() const { return generate_ts_patterns_; }

    /// Retrieve the list of participating inputs.
    std::vector<InterfaceSpecification> const inputs() const { return inputs_; }

    /// Retrieve the list of compute node outputs.
    std::vector<InterfaceSpecification> const outputs() const
    {
        return outputs_;
    }

    /// Return a list of input hosts.
    std::vector<std::string> const input_hosts() const
    {
        std::vector<std::string> hosts;
        hosts.reserve(inputs_.size());
        std::transform(
            inputs_.begin(), inputs_.end(), back_inserter(hosts),
            [](InterfaceSpecification input) { return input.host; });
        return hosts;
    }

    /// Return a list of input URIs.
    std::vector<std::string> const input_uris() const
    {
        std::vector<std::string> uris;
        uris.reserve(inputs_.size());
        std::transform(
            inputs_.begin(), inputs_.end(), back_inserter(uris),
            [](InterfaceSpecification input) { return input.full_uri; });
        return uris;
    }

    /// Return a list of compute node output hosts.
    std::vector<std::string> const output_hosts() const
    {
        std::vector<std::string> hosts;
        hosts.reserve(outputs_.size());
        std::transform(
            outputs_.begin(), outputs_.end(), back_inserter(hosts),
            [](InterfaceSpecification output) { return output.host; });
        return hosts;
    }

    /// Return a list of compute node output URIs.
    std::vector<std::string> const output_uris() const
    {
        std::vector<std::string> uris;
        uris.reserve(outputs_.size());
        std::transform(
            outputs_.begin(), outputs_.end(), back_inserter(uris),
            [](InterfaceSpecification output) { return output.full_uri; });
        return uris;
    }

    /// Retrieve this applications's indexes in the list of inputs.
    std::vector<unsigned> input_indexes() const { return input_indexes_; }

    /// Retrieve this applications's indexes in the list of outputs.
    std::vector<unsigned> output_indexes() const { return output_indexes_; }

private:
    /// Parse command line options.
    void parse_options(int argc, char* argv[]);

    /// The global timeslice size in number of microslices.
    uint32_t timeslice_size_ = 100;

    /// The global maximum timeslice number.
    uint32_t max_timeslice_number_ = UINT32_MAX;

    /// The name of the executable acting as timeslice processor.
    std::string processor_executable_;

    /// The number of instances of the timeslice processor executable.
    uint32_t processor_instances_ = 2;

    /// The global base port.
    uint32_t base_port_ = 20079;

    /// The selected transport implementation.
    Transport transport_ = Transport::RDMA;

    /// Generate patterns while generating embedded timeslices
    bool generate_ts_patterns_ = false;

    /// The list of participating inputs.
    std::vector<InterfaceSpecification> inputs_;

    /// The list of compute node outputs.
    std::vector<InterfaceSpecification> outputs_;

    /// This applications's indexes in the list of inputs.
    std::vector<unsigned> input_indexes_;

    /// This applications's indexes in the list of outputs.
    std::vector<unsigned> output_indexes_;
};
