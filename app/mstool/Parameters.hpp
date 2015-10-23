// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <stdexcept>
#include <cstdint>
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

    bool use_pattern_generator() const { return use_pattern_generator_; }

    uint32_t pattern_generator_type() const { return pattern_generator_type_; }

    std::string input_archive() const { return input_archive_; }

    std::string output_archive() const { return output_archive_; }

    bool analyze() const { return analyze_; }

    uint64_t maximum_number() const { return maximum_number_; }

    bool use_shared_memory() const { return use_shared_memory_; }

    size_t shared_memory_channel() const { return shared_memory_channel_; }

    std::string output_shm_identifier() const { return output_shm_identifier_; }

private:
    void parse_options(int argc, char* argv[]);

    bool use_pattern_generator_ = false;
    uint32_t pattern_generator_type_ = 0;
    std::string input_archive_;
    std::string output_archive_;
    bool analyze_ = false;
    uint64_t maximum_number_ = UINT64_MAX;
    bool use_shared_memory_ = false;
    size_t shared_memory_channel_ = 0;
    std::string output_shm_identifier_;
};
