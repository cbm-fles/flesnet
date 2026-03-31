#ifndef MSAWRITER_HPP
#define MSAWRITER_HPP

#include <boost/program_options.hpp>
#include <iostream>

// Flesnet Library header files:
#include "lib/fles_ipc/MicrosliceOutputArchive.hpp"
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

// tsa2msa header files:
#include "msaValidator.hpp"
#include "optionsGroup.hpp"
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
class msaWriterOptions : public optionsGroup {
public:
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
   * options are set.
   *
   */
  bool useSequence() const { return maxItemsPerArchive || maxBytesPerArchive; }

  std::vector<std::string> sys_ids;

  std::vector<std::string> eq_ids;

  std::string output_folder;

  bool create_folder;

  boost::program_options::options_description
  optionsDescription(bool hidden) override;

  /**
   * @brief Provides default msaWriterOptions.
   *
   * The default options are:
   * \snippet msaWriter.cpp msaWriterDefaults
   *
   * @return The default options for an msaWriter.
   */
  msaWriterOptions();
  ~msaWriterOptions() override = default;
};

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
  msaWriter() = default;

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
   * @brief Provides command line options descriptions for use with the
   * boost::program_options library, which then automatically updates
   * the msaWriterOptions object passed by reference as an argument.
   *
   * @details
   * In accordance with recommended practice,
   * the caller should call this function twice: once to obtain the
   * visible options and once to obtain the hidden options, which despite
   * not needed to be exposed to the user (unless explicitly requested),
   * are still needed to provide desired functionality.
   *
   * @param options The options description object serving a dual purpose:
   * its member variables are used as default values for the command line
   * options and the object itself is later on automatically updated by the
   * boost::program_options library with options provided
   * by the user on the command line.
   *
   * @param hidden Whether to return hidden or regular options. Hidden options
   * are additional options that are not shown in the help message unless
   * explicitly requested by specifying `--help` together with the `--verbose`
   * option.
   *
   * @return The command line options for the msaWriter. Can be combined
   * with other command line options and used to parse user input.
   *
   */
  static boost::program_options::options_description
  optionsDescription(msaWriterOptions& options, bool hidden);

  /**
   * @brief Writes contents of a timeslice to the microslice archives.
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
  void write(std::shared_ptr<fles::Timeslice> timeslice);

private:
  // A helper that is called by write(std::shared_ptr<fles::Timeslice>
  // timeslice) for each component in the timeslice.
  void write(std::shared_ptr<fles::Timeslice> timeslice, uint64_t tsc);

  // A helper that is called by write(std::shared_ptr<fles::Timeslice>
  // timeslice, uint64_t tsc) for each microslice in the component.
  void write(std::shared_ptr<fles::MicrosliceView> ms_ptr);

  std::string constructArchiveName(const fles::Subsystem& sys_id,
                                   const uint16_t& eq_id);

  // Other constructors are deleted because reasoning about what should
  // be the desired behaviour is non-trivial:
  msaWriter(const msaWriter& other) = delete;
  msaWriter& operator=(const msaWriter& other) = delete;
  msaWriter(msaWriter&& other) = delete;
  msaWriter& operator=(msaWriter&& other) = delete;
};

#endif // MSAWRITER_HPP
