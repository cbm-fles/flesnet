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
          return EX_SOFTWARE;
        }
        nTimeslices++;
        uint64_t timesliceIndex = timeslice->index();
        uint64_t numCoreMicroslices = timeslice->num_core_microslices();
        uint64_t numComponents = timeslice->num_components();
        uint64_t start_time = timeslice->start_time();
        if (options.beVerbose) {
          std::cout << "Timeslice index: " << timesliceIndex << std::endl;
          std::cout << "Number of core microslices: " << numCoreMicroslices
                    << std::endl;
          std::cout << "Number of components: " << numComponents << std::endl;
          std::cout << "Start time: " << start_time << std::endl;
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

    std::cout << "Number of timeslices: " << nTimeslices << std::endl;
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
