#ifndef MSAWRITER_HPP
#define MSAWRITER_HPP

#include <boost/program_options.hpp>
#include <iostream>

// Flesnet Library header files:
#include "lib/fles_ipc/MicrosliceOutputArchive.hpp"
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

// tsa2msa header files:
#include "msaValidator.hpp"
#include "utils.hpp"

/**
 * @file msaWriter.hpp
 * @brief Contains the msaWriter class and related functions and structs.
 */

/**
 * @struct msaWriterOptions
 * @brief Options that will be used by an msaWriter.
 *
 * @details The msaWriterOptions struct is a container for options that
 * are used by the msaWriter class. Using the function
 * defaults() an object with default values can be
 * created, and then modified as desired. Furthermore, using the
 * boost::program_options library and the function
 * getMsaWriterOptionsDescription(), the user-provided modifications can
 * be parsed from the command line and automatically applied to the
 * msaWriterOptions object.
 *
 * \note Technically, if any of the options maxItemsPerArchive or
 * maxBytesPerArchive is set to a non-zero value, then the Flesnet
 * library will set the other option as well to SIZE_MAX. Practically,
 * this is not going to change behaviour.
 *
 */
typedef struct msaWriterOptions {
  /**
   * @brief If set to true, the msaWriter will not perform any writes.
   *
   * @details Useful for checking the validity of the output without
   * acutally writing anything.
   */
  bool dryRun;

  /**
   * @brief If set to true, the msaWriter will print additional
   * information to the standard output.
   */
  bool beVerbose;

  /**
   * @brief If set to true, the msaWriter will stop at some points and
   * wait for user input before continuing.
   *
   * @details Useful for debugging purposes. The msaWriter will stop at some
   * points and wait for user input before continuing.
   *
   * \todo Please check the source code and remove hard-coded false
   * values to adjust the behaviour of the interactive mode until this
   * option is extended to have more than two states.
   *
   */
  bool interactive;

  /**
   * @brief A prefix to be used for the output files.
   *
   * @details The prefix is prepended to the names of the output files.
   * It is useful for distinguishing between different runs of the
   * msaWriter. Currently, if the prefix is set to an empty string, then
   * in main() the common prefix of the input files is used as the
   * prefix for the output files.
   *
   * \todo This calculation of the common prefix needs to be moved up in
   * the code to the point where the msaWriterOptions object is created.
   */
  std::string prefix;

  /**
   * @brief The maximum number of items per archive.
   *
   * @details If set to a non-zero value, the msaWriter will create
   * multiple archives if the number of items exceeds this limit. The
   * items are microslices in this case. If set to zero, there is no
   * limit.
   *
   * \todo Check whether the Flesnet library has an off-by-one error
   * when it comes to the number of items per archive, similarly how it
   * has an error with the limit for the number of bytes per archive.
   */
  std::size_t maxItemsPerArchive;

  /**
   * @brief The maximum number of bytes per archive.
   *
   * @details If set to a non-zero value, the msaWriter will create
   * multiple archives if the number of bytes exceeds this limit. If set
   * to zero, there is no limit. Currently, the Flesnet library is
   * incapable of correctly respecting this limit and will exceed it by
   * the size of one microslice.
   */
  bytesNumber maxBytesPerArchive; // zero means no limit

  /**
   * @brief Returns whether the OutputArchiveSequence classes should be used.
   *
   * @details If either maxItemsPerArchive or maxBytesPerArchive is set
   * to a non-zero value, then the OutputArchiveSequence classes should
   * be used. This is a helper function to determine whether these
   * options are set (and, if called for the first time, provides a
   * warning about the current state of the OutputArchiveSequence
   * classes in the Flesnet library).
   *
   */
  bool useSequence() const {
    static bool gaveWarning = false;
    if (!gaveWarning) {
      // TODO: Move this message somewhere else.
      std::cerr
          << "Warning: Currently, the OutputArchiveSequence"
             " classes do not properly handle the limits (at least not the"
             " maxBytesPerArchive limit; limits may be exceeded by the"
             " size of a micro slice.)"
          << std::endl;
      gaveWarning = true;
    }
    return maxItemsPerArchive || maxBytesPerArchive;
  }
} msaWriterOptions;



/**
 * @class msaWriter
 * @brief This class represents a writer for microslice archives.
 *
 * @details The msaWriter takes timeslices and writes their contents to
 * microslice archives. It is a wrapper around existing classes from the
 * Flesnet library, enhanced with some basic validation and options. Its
 * main method write_timeslice is intended to be called repeatedly with
 * (chronologically sorted) timeslices read from a source.
 *
 * \note For now, many default methods such as copy constructor, copy
 * assignment, move constructor, and move assignment are private and
 * deleted until they are needed (if ever). Similarly, the class is
 * marked as final to prevent inheritance (until needed).
 */
class msaWriter final {
  msaValidator validator;

  // TODO: Get rid of the unique_ptr and use emplace instead
  // TODO: Get rid of the string as a key and use some enum class or
  // tuple or something else as a key (which simultaneously could be
  // used to derive the string for the filename).
  // TODO: Explain implementation
  std::map<std::string, std::unique_ptr<fles::Sink<fles::Microslice>>> msaFiles;

  uint64_t numTimeslices = 0;
  const msaWriterOptions options;

public:
  /**
   * @brief Constructor for the msaWriter object using default options.
   *
   */
  msaWriter() : msaWriter(defaults()){};

  /**
   * @brief Constructs an msaWriter object with the specified options.
   *
   * @param options The options to be used by the msaWriter.
   */
  msaWriter(const msaWriterOptions& msaWriterOptions);

  /**
   * @brief Destructor for the msaWriter object.
   */
  ~msaWriter() = default;

  /**
   * @brief Provides default msaWriterOptions.
   *
   * The default options are:
   * \snippet msaWriter.cpp msaWriterDefaults
   *
   * @return The default options for an msaWriter.
   */
  static msaWriterOptions defaults();

  /**
   * @brief Provides command line options descriptions exclusive to the
   * msaWriter.
   *
   * @details Via the boost::program_options library, the object returned
   * by this function can be used to parse user provided command line
   * options for the msaWriter. In accordance with recommended practice,
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
   * @return The command line options for the msaWriter. Can be combined
   * with other command line options and used to parse user input.
   *
   * @note boost::program_options throws a runtime exception if multiple
   * options_description objects which contain the same option are
   * combined. Hence, name clashes should be avoided manually.
   */
  static boost::program_options::options_description
  optionsDescription(msaWriterOptions& options, bool hidden);

  /**
   * @brief Writes a timeslice to the microslice archives.
   *
   * @details The microslices contained in the timeslice are written to
   * microslice archives according to the options specified in the
   * msaWriterOptions object. If repeated calls to this function are
   * made, the contents of the timeslices are appended to the existing
   * microslice archives. Microslices Archives are sorted by their
   * equipment ID and Subsystem ID (which coincides with the timeslice
   * components within a timeslice) and named accordingly. Several
   * validation checks are performed on the timeslices and microslices
   * during the writing process (however, see the msaValidator class
   * for more details on the current state of validation).
   *
   * \pre If repeated calls to this function are made timeslices must be
   * passed in chronological order.
   *
   * \post The microslices contained in the microslice archives are
   * written in chronological order and the overlap between different
   * timeslices is discarded.
   *
   * @param timeslice The timeslice containing the microslices to be written to
   * the microslice archives.
   */
  void write_timeslice(std::shared_ptr<fles::Timeslice> timeslice);

private:
  void write_timeslice_component(std::shared_ptr<fles::Timeslice> timeslice,
                                 uint64_t tsc);

  std::string constructArchiveName(const fles::Subsystem& sys_id,
                                   const uint16_t& eq_id);

  void write_microslice(std::shared_ptr<fles::MicrosliceView> ms_ptr);

  // Delete copy constructor:
  msaWriter(const msaWriter& other) = delete;

  // Delete copy assignment:
  msaWriter& operator=(const msaWriter& other) = delete;

  // Delete move constructor:
  msaWriter(msaWriter&& other) = delete;

  // Delete move assignment:
  msaWriter& operator=(msaWriter&& other) = delete;
};

#endif // MSAWRITER_HPP
