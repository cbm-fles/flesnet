#ifndef MSAWRITER_HPP
#define MSAWRITER_HPP

#include <boost/program_options.hpp>
#include <iostream>

// FLESnet Library header files:
#include "lib/fles_ipc/MicrosliceOutputArchive.hpp"
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

// tsa2msa header files:
#include "msaValidator.hpp"

/**
 * @file msaWriter.hpp
 * @brief Contains the msaWriter class and related functions and structs.
 */

/**
 * @struct bytesNumber
 * @brief A wrapper around a std::size_t needed by boost::program_options.
 *
 * \todo Move this struct to `utils.hpp`.
 *
 * This struct merely a wrapper around a std::size_t needed by the
 * boost::program_options library to validate human-readable input for
 * the maxBytesPerArchive option in the msaWriterOption. This follows an
 * example provided in the boost::program_options documentation (of e.g.
 * boost version 1.85).
 */
struct bytesNumber {
  std::size_t value;
  bytesNumber(std::size_t value) : value(value) {}
  bytesNumber() : value(0) {}

  // Define the conversion operators following std::size_t behaviour.
  operator bool() const { return value; }
  operator std::size_t() const { return value; }
};

// Defining the << operator is necessary to allow the
// boost::program_options library to set default values (because it
// uses this operator to print an options description).
std::ostream& operator<<(std::ostream& os, const struct bytesNumber& bN);

/**
 * @brief Validate user input for the maxBytesPerArchive option.
 *
 * This function is only used by the boost::program_options library to
 * validate user input for the maxBytesPerArchive option in the
 * msaWriterOptions struct. It is not intended to be used directly.
 */
void validate(boost::any& v,
              const std::vector<std::string>& values,
              bytesNumber*,
              int);

/**
 * @struct msaWriterOptions
 * @brief Options that will be used by an msaWriter.
 *
 */
typedef struct msaWriterOptions {
  bool dryRun;
  bool beVerbose;
  bool interactive;
  std::string prefix;
  // Technically, the OutputSequence base classes, if used, do enforce
  // both of the following options. So, setting one of them to a
  // non-zero value (as of the time of writing), but not the other, will
  // behave as if the other value was set to SIZE_MAX. Practically, this
  // is not going to change behaviour.
  //
  // TODO: Comment at some appropriate place that setting one of these
  // results in the output file(s) being named with a sequence number,
  // regardless of whether a single file not exceeding the limits is
  // sufficient.
  std::size_t maxItemsPerArchive; // zero means no limit
  bytesNumber maxBytesPerArchive; // zero means no limit

  // TODO: Currently, the OutputArchiveSequence classes do not properly
  // handle the limits (at least not the maxBytesPerArchive limit).
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
 * @brief Returns the default options for an msaWriter.
 *
 * The default options are:
 * - dryRun     = false
 * - beVerbose  = false
 *
 * @return The default options for an msaWriter.
 */
msaWriterOptions defaultMsaWriterOptions();

/**
 * @brief Command line options exclusive to the msaWriter.
 *
 * Can be used to parse command line options for the msaWriter. These
 * options are necessarily exclusive to the msaWriter. Options shared
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
 *    --dry-run, -d corresponds to dryRun
 *
 *  Other options:
 *    None
 *
 * @param options The msaWriterOptions object to be used for boolean
 * switches, whose values are taken as defaults. Other options need
 * to be extracted from the variables_map.
 *
 * @param hidden Whether to return hidden or regular options. Hidden options are
 * additional options that are not shown in the help message unless explicitly
 * requested by specifying `--help` together with the `--verbose` option.
 *
 * @return The command line options for the msaWriter. Can be combined
 * with other command line options and used to parse user input. The
 * resulting variables_map can be used to construct an msaWriterOptions
 * object.
 *
 * @note boost::program_options throws an exception if multiple
 * options_description objects which contain the same option are
 * combined. Hence, in case of future name clashes, the
 * options_description must be adjusted and the shared option
 * defined somewhere else.
 */
boost::program_options::options_description
getMsaWriterOptionsDescription(msaWriterOptions& options, bool hidden);

/**
 * @brief Writes non switch options from the variables_map to the
 * msaWriterOptions object.
 *
 * The msaWriterOptions object is constructed based on the options present
 * in the variables_map object. Boolean switches, both exclusive to msaWriter
 * and shared ones, are ignored.
 *
 * @param vm The variables_map object that contains the command line options.
 * @return The msaWriterOptions object based on the command line options.
 *
 * @note Shared option which are boolean switches need to be set manually.
 * Exclusive boolean switches are set automatically.
 */
void getNonSwitchMsaWriterOptions(
    [[maybe_unused]] // Remove this line if the parameter is used
    const boost::program_options::variables_map& vm,
    [[maybe_unused]] // Remove this line if the parameter is used
    msaWriterOptions& msaWriterOptions);


/**
 * @class msaWriter
 * @brief This class represents a writer for micro slice archives.
 *
 * The msaWriter class provides functionality to write MSA data to a
 * file or other output stream. For now, many default methods such as
 * copy constructor, copy assignment, move constructor, and move
 * assignment are deleted until they are needed (if ever). Similarly,
 * the class is marked as final to prevent inheritance (until needed).
 */
class msaWriter final {
  msaValidator validator;

  // TODO: Get rid of the unique_ptr and use emplace instead
  // TODO: Get rid of the string as a key and use some enum class or
  // tuple or something else as a key (which simultaneously could be
  // used to derive the string for the filename).
  std::map<std::string, std::unique_ptr<fles::Sink<fles::Microslice>>> msaFiles;

  uint64_t numTimeslices = 0;
  uint64_t numMicroslices = 0;
  const msaWriterOptions options;

public:
  /**
   * @brief Constructor for the msaWriter object using default options.
   *
   */
  msaWriter() : msaWriter(defaultMsaWriterOptions()){};

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

  // Delete copy constructor:
  msaWriter(const msaWriter& other) = delete;

  // Delete copy assignment:
  msaWriter& operator=(const msaWriter& other) = delete;

  // Delete move constructor:
  msaWriter(msaWriter&& other) = delete;

  // Delete move assignment:
  msaWriter& operator=(msaWriter&& other) = delete;

  void write_timeslice(std::shared_ptr<fles::Timeslice> timeslice) {
    // TODO: Count sys_id changes per timeslice, check if there is new
    // or missing sys_ids.


    // Write the timeslice to a file:
    for (uint64_t tsc = 0; tsc < timeslice->num_components(); tsc++) {
      write_timeslice_component(timeslice, tsc);
    }

    // Inform the validator that the Timeslice has ended:
    validator.timesliceEnd(options.beVerbose);

    if (options.interactive) {
      std::cout << "Press Enter to continue..." << std::endl;
      std::cin.get();
    }
    numTimeslices++;
  }

  void write_timeslice_component(std::shared_ptr<fles::Timeslice> timeslice,
                                 uint64_t tsc) {
    // TODO: Count sys_id changes per TimesliceComponent, check if
    // there is new or missing sys_ids.

    // TODO: Check for num_core_microslices() changes.
    for (uint64_t msc = 0; msc < timeslice->num_core_microslices(); msc++) {
      std::unique_ptr<fles::MicrosliceView> ms_ptr =
          std::make_unique<fles::MicrosliceView>(
              timeslice->get_microslice(tsc, msc));
      write_microslice(std::move(ms_ptr));
    }

    // Inform the validator that the TimesliceComponent has ended:
    validator.timesliceComponentEnd(false && options.beVerbose);

    if (false && options.interactive) {
      std::cout << "Press Enter to continue..." << std::endl;
      std::cin.get();
    }
  }

  std::string constructArchiveName(const fles::Subsystem& sys_id,
                                   const uint16_t& eq_id) {

    // TODO: Do not construct the archive name for every microslice,
    // but do some caching instead.
    std::string prefix = options.prefix;

    if (prefix.size() == 0) {
      std::cerr << "Error: Prefix is empty, should not happen."
                << " Setting arbitrary prefix." << std::endl;
      prefix = "empty_prefix";
    }
    std::string sys_id_string = fles::to_string(sys_id);
    // eq_id is a uint16_t, and most likely typedefed to some
    // primitive integer type, so likely implicit conversion to
    // that integer type is safe. However, the fixed width
    // integer types are implementation defined, so the correct
    // way to do this likely involves using the PRIu16 format
    // macro.
    std::string eq_id_string = std::to_string(eq_id);
    std::string optionalSequenceIndicator =
        options.useSequence() ? "_%n" : "";

    std::string msa_archive_name = prefix + "_" + sys_id_string + "_" +
                                   eq_id_string + optionalSequenceIndicator +
                                   ".msa";
    // TODO: This happens for every microslice, which is really
    // unnecessary and should happen for every TimesliceComponent
    // instead. Only that the values stay constant per
    // TimesliceComponent needs to be checked here.
    if (msaFiles.find(msa_archive_name) == msaFiles.end()) {
      std::unique_ptr<fles::Sink<fles::Microslice>> msaFile;
      if (options.useSequence()) {
        msaFile = std::make_unique<fles::MicrosliceOutputArchiveSequence>(
            msa_archive_name, options.maxItemsPerArchive,
            options.maxBytesPerArchive);
      } else {
        msaFile =
            std::make_unique<fles::MicrosliceOutputArchive>(msa_archive_name);
      }
      msaFiles[msa_archive_name] = std::move(msaFile);
    }
    return msa_archive_name;
  }

  void write_microslice(std::shared_ptr<fles::MicrosliceView> ms_ptr) {
    const fles::MicrosliceDescriptor& msd = ms_ptr->desc();

    // TODO: Take into account that raw values not corresponding to
    // some named enum value should be treated as an error.
    const uint16_t& eq_id = msd.eq_id;
    const fles::Subsystem& sys_id = static_cast<fles::Subsystem>(msd.sys_id);

    validator.check_microslice(ms_ptr, false && options.beVerbose);

    if (!options.dryRun) {
      std::string msa_archive_name = constructArchiveName(sys_id, eq_id);
      msaFiles[msa_archive_name]->put(std::move(ms_ptr));
    }

    if (false && options.interactive) {
      std::cout << "Press Enter to continue..." << std::endl;
      std::cin.get();
    }
  }

};

#endif // MSAWRITER_HPP
