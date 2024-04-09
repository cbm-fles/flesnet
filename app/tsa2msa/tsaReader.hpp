#ifndef TSAREADER_HPP
#define TSAREADER_HPP

// C++ Standard Library header files:
#include <chrono>
#include <iostream>

// System dependent header files:
#include <sysexits.h>

// Boost Library header files:
#include <boost/program_options.hpp>

// FLESnet Library header files:
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

/**
 * @struct tsaReaderOptions
 * @brief Options that will be used by a tsaReader.
 */
typedef struct tsaReaderOptions {
  bool beVerbose;
  bool interactive;
  std::vector<std::string> input;
  std::string readingMethod;
} tsaReaderOptions;

/**
 * @brief Returns the default options for an tsaReader.
 *
 * The default options are:
 * - beVerbose  = false
 * - input      = empty vector
 * The input vector must be populated with input by the user.
 *
 * @note TODO: Describe what is supposed to happen with the input vector and how
 * it should get populated.
 *
 * @return The default options for an tsaReader.
 */
tsaReaderOptions defaultTsaReaderOptions();

/**
 * @brief Command line options exclusive to the tsaReader.
 *
 * Can be used to parse named command line options for the tsaReader. These
 * options are necessarily exclusive to the tsaReader. Options shared
 * with other classes need to be handled separately. Currently, we have
 * the following options:
 *
 * Shared options:
 *  Boolean switches:
 *    --verbose, -v corresponds to beVerbose
 *
 *  Other options:
 *    None
 *
 * Exclusive options:
 *  Boolean switches:
 *    None
 *
 *  Other options:
 *    --input, -i corresponds to input
 *
 * @note The input option is used as a helper for interpreting all positional
 * arguments as input files. The corresponding
 * boost::program_options::positional_options_description object must be
 * created and used separately. Since positional arguments, by their very
 * nature, are very sensitive to the number and order of the options, they
 * are all handled in the main function.
 *
 * @param options The tsaReaderOptions object to be used for tsaReader options,
 * whose values are taken as defaults.
 *
 * @param hidden Whether to return hidden or regular options. Hidden options are
 * additional options that are not shown in the help message unless explicitly
 * requested by specifying `--help` together with the `--verbose` option.
 *
 * @return The named command line options for the tsaReader. Can be combined
 * with other command line options and used to parse user input. The
 * resulting variables_map can be used to construct an tsaReaderOptions
 * object.
 *
 * @note boost::program_options throws an exception if multiple
 * options_description objects which contain the same option are
 * combined. Hence, in case of future name clashes, the
 * options_description must be adjusted and the shared option
 * defined somewhere else.*/
boost::program_options::options_description
getTsaReaderOptionsDescription(tsaReaderOptions& options, bool hidden);

/**
 * @brief Parses the command line options for the tsaReader.
 *
 * @param vm The variables_map object populated by the command line
 * options given by the user. It is assumed that it was parsed using the
 * options_description object returned by getTsaReaderOptionsDescription
 * called with the same tsaReaderOptions object as the one passed to
 * this function.
 * @param tsaReaderOptions The tsaReaderOptions object to be populated
 * with the values given by the user. It is assumed that it was used to
 * populate the variables_map object given to this function.
 */
void getTsaReaderOptions(const boost::program_options::variables_map& vm,
                         tsaReaderOptions& tsaReaderOptions);

/**
 * @class tsaReaderValidator
 * @brief This class validates the Timeslices read by a tsaReader.
 *
 * The tsaReaderValidator provides functionality to validate the Timeslices
 * read by a tsaReader.
 *
 *
 */
class tsaReaderValidator {

public:
  /**
   * @brief Constructor for the tsaReaderValidator object.
   */
  tsaReaderValidator()
      : time_monotonically_increasing(true), time_strict_monotonicity(true),
        index_monotonically_increasing(true), index_strict_monotonicity(true),
        num_core_microslices_constant(true), num_components_constant(true),

        nStrictMonotonicityViolationsTime(0),
        nStrictMonotonicityViolationsIndex(0), nMonotonicityViolationsTime(0),
        nMonotonicityViolationsIndex(0), nNumCoreMicroslicesViolations(0),
        nNumComponentsViolations(0),

        last_timeslice_start_time(0), last_timeslice_index(0),
        last_timeslice_num_core_microslices(0),
        last_timeslice_num_components(0),

        nTimeslices(0){};

  /**
   * @brief Destructor for the tsaReaderValidator object.
   */
  ~tsaReaderValidator() = default;

  // Delete copy constructor:
  tsaReaderValidator(const tsaReaderValidator& other) = delete;

  // Delete copy assignment:
  tsaReaderValidator& operator=(const tsaReaderValidator& other) = delete;

  // Delete move constructor:
  tsaReaderValidator(tsaReaderValidator&& other) = delete;

  // Delete move assignment:
  tsaReaderValidator& operator=(tsaReaderValidator&& other) = delete;

  /**
   * @brief Validates the Timeslices read by a tsaReader and their
   * order.
   *
   * @return EX_OK if successful, EX_SOFTWARE if an error occurred.
   */
  int validate(const std::unique_ptr<fles::Timeslice>& timeslice) {
    if (!timeslice) {
      std::cerr << "Error: No timeslice received." << std::endl;
      return EX_SOFTWARE;
    }

    uint64_t timesliceIndex = timeslice->index();
    uint64_t numCoreMicroslices = timeslice->num_core_microslices();
    uint64_t numComponents = timeslice->num_components();
    uint64_t start_time = timeslice->start_time();
    if (nTimeslices != 0) {
      if (start_time < last_timeslice_start_time) {
        time_monotonically_increasing = false;
        time_strict_monotonicity = false;
        nMonotonicityViolationsTime++;
        nStrictMonotonicityViolationsTime++;
      } else if (start_time == last_timeslice_start_time) {
        time_strict_monotonicity = false;
        nStrictMonotonicityViolationsTime++;
      }
    }
    last_timeslice_start_time = start_time;

    if (nTimeslices != 0) {
      if (timesliceIndex < last_timeslice_index) {
        index_monotonically_increasing = false;
        index_strict_monotonicity = false;
        nMonotonicityViolationsIndex++;
        nStrictMonotonicityViolationsIndex++;
      } else if (timesliceIndex == last_timeslice_index) {
        index_strict_monotonicity = false;
        nStrictMonotonicityViolationsIndex++;
      }
    }
    last_timeslice_index = timesliceIndex;

    if (nTimeslices != 0 &&
        numCoreMicroslices != last_timeslice_num_core_microslices) {
      num_core_microslices_constant = false;
      nNumCoreMicroslicesViolations++;
    }
    last_timeslice_num_core_microslices = numCoreMicroslices;
    if (nTimeslices != 0 && numComponents != last_timeslice_num_components) {
      num_components_constant = false;
      nNumComponentsViolations++;
    }
    last_timeslice_num_components = numComponents;
    nTimeslices++;

    return EX_OK;
  }

  void printVerboseIntermediateResult() {
    std::cout << "Timeslice number: " << nTimeslices << std::endl;
    std::cout << "Timeslice index: " << last_timeslice_index << std::endl;
    std::cout << "Number of core microslices: "
              << last_timeslice_num_core_microslices << std::endl;
    std::cout << "Number of components: " << last_timeslice_num_components
              << std::endl;
    std::cout << "Start time: " << last_timeslice_start_time << std::endl;
    if (!time_monotonically_increasing) {
      std::cout << "Warning: Timeslice start time is not monotonically "
                   "increasing."
                << std::endl;
      std::cout << "Number of monotonicity violations: "
                << nMonotonicityViolationsTime << std::endl;
    } else if (!time_strict_monotonicity) {
      std::cout << "Error: Timeslice start time is not strictly "
                   "monotonically increasing."
                << std::endl;
      std::cout << "Number of strict monotonicity violations: "
                << nStrictMonotonicityViolationsTime << std::endl;
    }
    if (!index_monotonically_increasing) {
      std::cout << "Warning: Timeslice index is not monotonically increasing."
                << std::endl;
      std::cout << "Number of monotonicity violations: "
                << nMonotonicityViolationsIndex << std::endl;
    } else if (!index_strict_monotonicity) {
      std::cout << "Error: Timeslice index is not strictly monotonically "
                   "increasing."
                << std::endl;
      std::cout << "Number of strict monotonicity violations: "
                << nStrictMonotonicityViolationsIndex << std::endl;
    }
    if (!num_core_microslices_constant) {
      std::cout << "Warning: Number of core microslices is not constant."
                << std::endl;
      std::cout << "Number of violations: " << nNumCoreMicroslicesViolations
                << std::endl;
    }
    if (!num_components_constant) {
      std::cout << "Warning: Number of components is not constant."
                << std::endl;
      std::cout << "Number of violations: " << nNumComponentsViolations
                << std::endl;
    }
  }

  void printSummary() {

    std::cout << "Number of timeslices: " << nTimeslices << std::endl;
    if (!time_monotonically_increasing) {
      std::cerr
          << "Warning: Timeslice start time is not monotonically increasing."
          << std::endl;
    } else if (!time_strict_monotonicity) {
      std::cerr << "Error: Timeslice start time is not strictly monotonically "
                   "increasing."
                << std::endl;
    }
    if (!index_monotonically_increasing) {
      std::cerr << "Warning: Timeslice index is not monotonically increasing."
                << std::endl;
    } else if (!index_strict_monotonicity) {
      std::cerr << "Error: Timeslice index is not strictly monotonically "
                   "increasing."
                << std::endl;
    }
  }

private:
  // true until proven false:
  bool time_monotonically_increasing;
  bool time_strict_monotonicity;
  bool index_monotonically_increasing;
  bool index_strict_monotonicity;
  bool num_core_microslices_constant;
  bool num_components_constant;

  // number of violations:
  uint64_t nStrictMonotonicityViolationsTime;
  uint64_t nStrictMonotonicityViolationsIndex;
  uint64_t nMonotonicityViolationsTime;
  uint64_t nMonotonicityViolationsIndex;
  uint64_t nNumCoreMicroslicesViolations;
  uint64_t nNumComponentsViolations;

  // last values, used for comparison with current values:
  uint64_t last_timeslice_start_time;
  uint64_t last_timeslice_index;
  uint64_t last_timeslice_num_core_microslices;
  uint64_t last_timeslice_num_components;
  uint64_t nTimeslices;
};

/**
 * @class tsaReader
 * @brief This class represents a reader for tsa archives.
 *
 * The tsaReader provides functionality to read TSA data from a file or
 * other input stream. For now, many default methods such as
 * copy constructor, copy assignment, move constructor, and move
 * assignment are deleted until they are needed (if ever). Similarly,
 * the class is marked as final to prevent inheritance (until needed).
 */
class tsaReader final {
private:
  tsaReaderOptions options;
  tsaReaderValidator validator;

public:
  /**
   *
   * @brief Constructor for the tsaReader object.
   *
   * @note If given invalid options, the constructor will throw an
   * exception.
   *
   * @param options The options to be used for the tsaReader, including
   * the input files to be read. Passed by value since we do not allow
   * the caller to change the options after the object is created.
   */
  explicit tsaReader(const tsaReaderOptions options) : options(options) {
    if (options.readingMethod != "auto") {
      throw std::runtime_error("Invalid reading method");
    }

    // TODO: Check the input file(s) for existence and readability here.
    // If a reading method is used that allows for complex patterns,
    // such as TimesliceAutoSource, their expansion should taken into
    // consideration.
  };

  // Delete default constructor (options must be provided):
  tsaReader() = delete;

  /**
   * @brief Destructor for the tsaReader object.
   */
  ~tsaReader() = default;

  // Delete copy constructor:
  tsaReader(const tsaReader& other) = delete;

  // Delete copy assignment:
  tsaReader& operator=(const tsaReader& other) = delete;

  // Delete move constructor:
  tsaReader(tsaReader&& other) = delete;

  // Delete move assignment:
  tsaReader& operator=(tsaReader&& other) = delete;

  /**
   * @brief Reads the input file(s) and prints the number of timeslices.
   *
   * @note Since checking the validity of the input during construction
   * is not yet implemented, this method may throw an exception if the
   * input is invalid. This should be fixed in the future.
   *
   * @return EX_OK if successful, EX_SOFTWARE if an error occurred.
   */
  int read() {
    // Join the input files into a ;-separated string:
    std::string input = "";
    for (const auto& i : options.input) {
      input += i;
      if (i != options.input.back()) {
        input += ";";
      }
    }

    std::unique_ptr<fles::TimesliceSource> source =
        std::make_unique<fles::TimesliceAutoSource>(input);

    int nTimeslices = 0;
    try {
      bool eos = source->eos();
      if (options.beVerbose) {
        std::cout << "Initial eos: " << eos << std::endl;
      }
      // get time to measure performance:
      auto start = std::chrono::steady_clock::now();
      while (!eos) {
        [[maybe_unused]] std::unique_ptr<fles::Timeslice> timeslice =
            source->get();
        if (!timeslice) {
          std::cerr << "Error: No timeslice received." << std::endl;
          std::cout << "Next eos: " << source->eos() << std::endl;
          bool strict_eos = false;
          if (strict_eos) {
            return EX_SOFTWARE;
          } else {
            break;
          }
        }
        nTimeslices++;
        validator.validate(timeslice);
        if (options.beVerbose) {
          validator.printVerboseIntermediateResult();
        }

        if (options.interactive) {
          // Wait for user input to continue:
          std::string dummy;
          std::cout << "Press Enter to continue..." << std::endl;
          std::getline(std::cin, dummy);
        }
        eos = source->eos();
        if (options.beVerbose) {
          std::cout << "Next eos: " << eos << std::endl;
        }
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        auto per_item = elapsed_seconds.count() / nTimeslices;
        if (options.beVerbose) {
          std::cout << "Elapsed time: " << elapsed_seconds.count() << "s"
                    << std::endl;
          std::cout << "Time per item: " << per_item << "s" << std::endl;
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EX_SOFTWARE;
    }
    validator.printSummary();

    return EX_OK;
  }
};

/**
 * @brief Returns the number of tsaReader options with defaults.
 *
 * This function is used to determine the number of options that are
 * passed on the command line. This number needs to be manually updated
 * whenever such an option is added or removed.
 *
 * @return The number of tsaReader options with defaults.
 */
unsigned int TsaReaderNumberOfOptionsWithDefaults();

#endif // TSAREADER_HPP
