// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <map>
#include <memory>
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

/// InputInterface enum.
enum class InputInterfaceScheme { SharedMemory, PatternGenerator };

/// OutputInterface enum.
enum class OutputInterfaceScheme { SharedMemory };

struct InputOutputParameters {
  template <class T>
  void add_parameter(T& var, const std::string name, const std::string help) {
    description +=
        "  " + name + "=" + std::to_string(var) + "   " + help + "\n";
  };
  template <class T>
  void
  add_path_component(T& var, std::size_t position, const std::string help) {
    description += "  pc" + std::to_string(position) + "   " + help + "\n";
  };

  std::string description;
  virtual void setup(){};
  virtual ~InputOutputParameters(){};
};

struct OutputParameters : InputOutputParameters {
  virtual void setup() { InputOutputParameters::setup(); };
  virtual ~OutputParameters(){};
};

struct ShmOutputParameters : OutputParameters {
  std::string shm_identifier;
  uint32_t datasize = 27; // 128 MiB
  uint32_t descsize = 19; // 16 MiB

  static const std::string help_string;

  virtual void setup() {
    // OutputParameters::setup();
    add_path_component(shm_identifier, 0, "shared memory identifier string");
    add_parameter(datasize, "datasize",
                  "(log2 of) the output data buffer size");
    add_parameter(descsize, "descsize",
                  "(log2 of) the output descriptor buffer size");
  };
  virtual ~ShmOutputParameters(){};
};

struct InputParameters : InputOutputParameters {
  uint32_t overlap_size = 1;

  void setup() {
    InputOutputParameters::setup();
    add_parameter(overlap_size, "overlap",
                  "number of microslices in overlap region between timeslices");
  };
  virtual ~InputParameters(){};
};

struct ShmInputParameters : InputParameters {
  std::string shm_identifier;
  uint32_t channel;

  void setup() {
    InputParameters::setup();
    add_path_component(shm_identifier, 0, "shared memory identifier string");
    add_path_component(channel, 1, "shared memory channel");
  };
  virtual ~ShmInputParameters(){};
};

struct PgenInputParameters : InputParameters {
  uint32_t datasize = 27;    // 128 MiB
  uint32_t descsize = 19;    // 16 MiB
  uint32_t size_mean = 1024; // 1 kiB
  uint32_t size_var = 0;
  uint32_t pattern = 0;
  uint64_t delay_ns = 0;

  void setup() {
    InputParameters::setup();
    add_parameter(datasize, "datasize",
                  "(log2 of) the pattern generator data buffer size");
    add_parameter(descsize, "descsize",
                  "(log2 of) the pattern generator descriptor buffer size");
    add_parameter(size_mean, "size_mean",
                  "mean size of the microslices (in Byte)");
    add_parameter(size_var, "size_var",
                  "variance in size of the microslices (in Byte)");
    add_parameter(pattern, "pattern", "pattern type (0: uninitialized)");
    add_parameter(delay_ns, "delay_ns",
                  "delay for given minimum time between each microslice, "
                  "inverse of microslice rate (in ns)");
  };
  virtual ~PgenInputParameters(){};
};

struct InputInterfaceSpecification : InterfaceSpecification {
  InputInterfaceScheme input_scheme;
  std::unique_ptr<InputParameters> input_param;
};

struct OutputInterfaceSpecification : InterfaceSpecification {
  OutputInterfaceScheme output_scheme;
  std::unique_ptr<OutputParameters> output_param;
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
  [[nodiscard]] std::vector<InputInterfaceSpecification> const& inputs() const {
    return inputs_;
  }

  /// Retrieve the list of compute node outputs.
  [[nodiscard]] std::vector<OutputInterfaceSpecification> const& outputs() const {
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

  void help_schemes();

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
  std::vector<InputInterfaceSpecification> inputs_;

  /// The list of compute node outputs.
  std::vector<OutputInterfaceSpecification> outputs_;

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
