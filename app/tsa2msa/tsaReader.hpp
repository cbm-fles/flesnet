#ifndef TSAREADER_HPP
#define TSAREADER_HPP

// C++ Standard Library header files:
#include <chrono>
#include <iostream>

// System dependent header files:
#include <sysexits.h>

// Boost Library header files:
#include <boost/program_options.hpp>

// Flesnet Library header files:
#include "lib/fles_ipc/MicrosliceOutputArchive.hpp"
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

// tsa2msa Library header files:
#include "tsaValidator.hpp"

/**
 * @file tsaReader.hpp
 * @brief This file contains the declarations of the tsaReader class and
 * related functions.
 */

/**
 * @struct tsaReaderOptions
 * @brief Options that will be used by a tsaReader.
 *
 * @details The tsaReaderOptions struct contains the options that will
 * be used by a tsaReader. Using the function defaultTsaReaderOptions()
 * the caller can obtain an object with default values for the options.
 * Furthermore, using the boost::program_options library and the
 * functions getTsaReaderOptionsDescription(), the user-provided
 * modifications can be parsed from the command line and automatically
 * applied to the tsaReaderOptions object.
 *
 */
typedef struct tsaReaderOptions {
  bool beVerbose;
  /**
   * @brief Whether the tsaReader should stop at each timeslice and wait for
   * the user to confirm before continuing.
   *
   * @details If interactive is set to true, the tsaReader will stop
   * after each Timeslice read and wait for the user to press enter
   * before continuing. This is useful for debugging and testing.
   */
  bool interactive;

  /**
   * @brief The inputs to be read by the tsaReader.
   *
   * @details The inputs files are given as a vector of strings.
   * Typically, each string represents a path to a file to be read by
   * the tsaReader. The Flesnet library, to which the input is passed,
   * allows for other input to be given as well. However, it is
   * recommended to simply pass the paths to `.tsa` files to be read.
   */
  std::vector<std::string> input;

  /**
   * @brief The method to call into the Flesnet library to read the
   * input.
   * @details Currently, the only supported method is "auto". However,
   * this is likely to change in the future as the corresponding Flesnet
   * library class is expected to be to memory consuming to be used in
   * the intended application of tsa2msa.
   */
  std::string readingMethod;
} tsaReaderOptions;

/**
 * @brief Returns the default options for an tsaReader.
 *
 * The default options are:
 * - beVerbose  = false
 * - interactive = false
 * - input      = empty vector
 *
 * The input vector is expected to be populated by the caller before the
 * tsaReaderOptions object is used to create a tsaReader.
 *
 * @note Unfortunately, the above list is not automatically updated when
 * the default options are changed in the code. This could be overcome
 * by using a constant global variable instead of a function. However,
 * the formatting of the doxygen documentation does not include the
 * struct member variable names in the description and does not look
 * very nice. Hence, the current solution is a compromise.
 *
 * @return The default options for an tsaReader.
 */
tsaReaderOptions defaultTsaReaderOptions();

/**
 * @brief Create command line options descriptions exclusive to the tsaReader.
 *
 * @details Via the boost::program_options library, the object returned
 * by this function can be used to parse user provided command line
 * options for the tsaReader. In accordance with recommended practice,
 * the caller should call this function twice: once to obtain the
 * visible options and once to obtain the hidden options, which despite
 * not needed to be exposed to the user (unless explicitly requested),
 * are still needed to provide desired functionality.
 *
 * @param options The options description object serving a dual purpose:
 * its member variables are used as default values for the command line
 * options and the object itself is used to store the options provided
 * by the user. These are set automatically by the
 * boost::program_options library.
 *
 * @param hidden Whether to return hidden or regular options. Hidden options are
 * additional options that are not shown in the help message unless explicitly
 * requested by specifying `--help` together with the `--verbose` option.
 *
 * @return The command line options for the tsaReader. Can be combined
 * with other command line options and used to parse user input.
 *
 * \todo Provide some template for these kind of functions to get rid of
 * the duplication of the doxygen description.
 *
 * @note boost::program_options throws a runtime exception if multiple
 * options_description objects which contain the same option are
 * combined. Hence, name clashes should be avoided manually.
 */
boost::program_options::options_description
getTsaReaderOptionsDescription(tsaReaderOptions& options, bool hidden);

/**
 * @brief Parses the command line options for the tsaReader.
 *
 * Currently, not all options in the tsaReaderOptions object are
 * automatically set by the boost::program_options library. Until this
 * is fixed, the user must call this function to populate the remaining
 * values using the variables_map object returned by the library. The
 * user must ensure to use the same tsaReaderOptions object to create the
 * options_description object used to parse the command line options and
 * populate the variables_map object as the one passed to this function.
 *
 * \todo Fix the issue that not all options are automatically set by the
 * boost::program_options library.
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
 * The tsaReader provides functionality to read from multiple timeslices
 * archives (or other input streams) and provide the timeslices in
 * chronological order to the caller. Furthermore, it provides basic
 * validation of the timeslices read and means to print basic
 * statistics. The main method read() is supposed to be called
 * repeatedly until it returns a nullptr, indicating that the end of the
 * input has been reached.
 *
 * The tsaReader calls into the Flesnet library to read the timeslices,
 * overcoming (or at least documenting) some of the limitations of the
 * library.
 *
 * \note For now, many default methods such as
 * copy constructor, copy assignment, move constructor, and move
 * assignment are deleted until they are needed (if ever). Similarly,
 * the class is marked as final to prevent inheritance (until needed).
 */
class tsaReader final {
private:
  tsaReaderOptions options;
  tsaValidator validator;
  std::unique_ptr<fles::TimesliceSource> source;
  uint64_t nTimeslices;

  std::chrono::duration<double> last_duration;
  std::chrono::duration<double> total_duration;

public:
  /**
   *
   * @brief Constructor for the tsaReader object.
   *
   * @note If given invalid options, the constructor will throw an
   * exception. This is a little unfortunate, but seems to be common
   * practice in C++ whenever not all possible inputs are valid.
   * Alternatively, we could require the user to check the options
   * before calling the constructor, but this does not really solve the
   * problem.
   *
   * @param options The options to be used for the tsaReader, including
   * the input files to be read. Passed by value since we do not allow
   * the caller to change the options after the object is created.
   */
  explicit tsaReader(const tsaReaderOptions options);

  /**
   * @brief Deleted default constructor for the tsaReader object.
   * @details In contrast to the default msaReader constructor, the
   * tsaReader is reliant on the input files given in the options, hence
   * a default constructor is not possible.
   */
  tsaReader() = delete;

  /**
   * @brief Destructor for the tsaReader object.
   */
  ~tsaReader() = default;

  /**
   * @brief Reads the next Timeslice from the input.
   *
   * @return A unique pointer to the Timeslice read from the input.
   * If the end of the input has been reached, a nullptr is returned.
   */
  std::unique_ptr<fles::Timeslice> read();

private:
  // Delete copy constructor:
  tsaReader(const tsaReader& other) = delete;

  // Delete copy assignment:
  tsaReader& operator=(const tsaReader& other) = delete;

  // Delete move constructor:
  tsaReader(tsaReader&& other) = delete;

  // Delete move assignment:
  tsaReader& operator=(tsaReader&& other) = delete;

  std::unique_ptr<fles::Timeslice> handleEosError();

  void printDuration();

private:
  void setSource(std::unique_ptr<fles::TimesliceSource> s);

  void initAutoSource(const std::vector<std::string>& inputs);

  void initSource(const std::vector<std::string>& inputs);
};

#endif // TSAREADER_HPP
