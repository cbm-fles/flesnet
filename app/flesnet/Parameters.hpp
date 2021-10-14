// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

/// Run parameter exception class.
/** A ParametersException object signals an error in a given parameter
    on the command line or in a configuration file. */

class ParametersException : public std::runtime_error {
public:
  /// The ParametersException constructor.
  explicit ParametersException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
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

class Parameters {
public:
  /// The Parameters command-line parsing constructor.
  Parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  Parameters(const Parameters&) = delete;
  void operator=(const Parameters&) = delete;

  [[nodiscard]] std::string monitor_uri() const { return monitor_uri_; }

  /// Retrieve the global timeslice size in number of microslices.
  [[nodiscard]] uint32_t timeslice_size() const { return timeslice_size_; }

  /// Retrieve the global maximum timeslice number.
  [[nodiscard]] uint32_t max_timeslice_number() const {
    return max_timeslice_number_;
  }

  /// Retrieve the name of the executable acting as timeslice processor.
  [[nodiscard]] std::string processor_executable() const {
    return processor_executable_;
  }

  /// Retrieve the number of instances of the timeslice processor executable.
  [[nodiscard]] uint32_t processor_instances() const {
    return processor_instances_;
  }

  /// Retrieve the global base port.
  [[nodiscard]] uint32_t base_port() const { return base_port_; }

  /// Retrieve the selected transport implementation.
  [[nodiscard]] Transport transport() const { return transport_; }

  /// Retrieve the list of participating inputs.
  [[nodiscard]] std::vector<InterfaceSpecification> inputs() const {
    return inputs_;
  }

  /// Retrieve the list of compute node outputs.
  [[nodiscard]] std::vector<InterfaceSpecification> outputs() const {
    return outputs_;
  }

  /// Retrieve this applications's indexes in the list of inputs.
  [[nodiscard]] std::vector<unsigned> input_indexes() const {
    return input_indexes_;
  }

  /// Retrieve this applications's indexes in the list of outputs.
  [[nodiscard]] std::vector<unsigned> output_indexes() const {
    return output_indexes_;
  }

  [[nodiscard]] bool local_only() const {
    return input_indexes_.size() == inputs_.size() &&
           output_indexes_.size() == outputs_.size();
  }

  /// flag to check whether to drop timeslice processing
  [[nodiscard]] bool drop_process_ts() const { return drop_process_ts_; }

  /// Retrieve the history size of intervals that the scheduler would decide
  /// based on
  [[nodiscard]] uint32_t scheduler_history_size() const {
    return scheduler_history_size_;
  }

  /// Retrieve the initial timeslices of each interval
  [[nodiscard]] uint32_t scheduler_interval_length() const {
    return scheduler_interval_length_;
  }

  /// Retrieve the maximum difference percentage between the proposed and the
  /// actual duration to apply the scheduler_speedup_percentage_
  [[nodiscard]] uint32_t scheduler_speedup_difference_percentage() const {
    return scheduler_speedup_difference_percentage_;
  }

  /// Retrieve the speeding up percentage of the scheduler to reduce the
  /// duration
  [[nodiscard]] uint32_t scheduler_speedup_percentage() const {
    return scheduler_speedup_percentage_;
  }

  /// Retrieve the speeding up interval count of the scheduler
  [[nodiscard]] uint32_t scheduler_speedup_interval_count() const {
    return scheduler_speedup_interval_count_;
  }

  /// Retrieve the directory to store DFS log files
  [[nodiscard]] std::string scheduler_log_directory() const {
    return scheduler_log_directory_;
  }

  /// Check whether to generate  DFS log files
  [[nodiscard]] bool scheduler_enable_logging() const {
    return scheduler_enable_logging_;
  }

private:
  /// Parse command line options.
  void parse_options(int argc, char* argv[]);

  std::string monitor_uri_;

  /// The global timeslice size in number of microslices.
  uint32_t timeslice_size_ = 100;

  /// The global maximum timeslice number.
  uint32_t max_timeslice_number_ = UINT32_MAX;

  /// The name of the executable acting as timeslice processor.
  std::string processor_executable_;

  /// The number of instances of the timeslice processor executable.
  uint32_t processor_instances_ = 1;

  /// The global base port.
  uint32_t base_port_ = 20079;

  /// The selected transport implementation.
  Transport transport_ = Transport::RDMA;

  /// The list of participating inputs.
  std::vector<InterfaceSpecification> inputs_;

  /// The list of compute node outputs.
  std::vector<InterfaceSpecification> outputs_;

  /// This applications's indexes in the list of inputs.
  std::vector<unsigned> input_indexes_;

  /// This applications's indexes in the list of outputs.
  std::vector<unsigned> output_indexes_;

  /// flag to check whether to drop timeslice processing
  bool drop_process_ts_ = false;

  /// This history size of intervals that the scheduler would decide based on
  uint32_t scheduler_history_size_{};

  /// The intial TSs of each interval of the scheduler
  uint32_t scheduler_interval_length_{};

  /// The maximum difference percentage between the proposed and the actual
  /// duration to apply the scheduler_speedup_percentage_
  uint32_t scheduler_speedup_difference_percentage_{};

  /// The speeding up percentage of the scheduler to reduce the duration
  uint32_t scheduler_speedup_percentage_{};

  /// The speeding up interval count of the scheduler
  uint32_t scheduler_speedup_interval_count_{};

  /// The directory to store the log files
  std::string scheduler_log_directory_;

  bool scheduler_enable_logging_ = false;
};
