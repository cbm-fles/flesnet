// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

/// Run parameter exception class.
class ParametersException : public std::runtime_error
{
public:
    explicit ParametersException(const std::string& what_arg = "")
        : std::runtime_error(what_arg)
    {
    }
};

/// Global run parameter class.
class Parameters
{
public:
    Parameters(int argc, char* argv[]) { parse_options(argc, argv); }

    Parameters(const Parameters&) = delete;
    void operator=(const Parameters&) = delete;

    int32_t client_index() const { return client_index_; }

    std::string shm_identifier() const { return shm_identifier_; }

    std::string input_archive() const { return input_archive_; }

    std::string output_archive() const { return output_archive_; }

    size_t output_archive_size() const { return output_archive_size_; }

    bool analyze() const { return analyze_; }

    bool benchmark() const { return benchmark_; }

    size_t verbosity() const { return verbosity_; }

    std::string publish_address() const { return publish_address_; }

    std::string subscribe_address() const { return subscribe_address_; }

    uint64_t maximum_number() const { return maximum_number_; }

private:
    void parse_options(int argc, char* argv[]);

    int32_t client_index_ = -1;
    std::string shm_identifier_;
    std::string input_archive_;
    std::string output_archive_;
    size_t output_archive_size_ = SIZE_MAX;
    bool analyze_ = false;
    bool benchmark_ = false;
    size_t verbosity_ = 0;
    std::string publish_address_;
    std::string subscribe_address_;
    uint64_t maximum_number_ = UINT64_MAX;
};
