#ifndef MSAWRITER_HPP
#define MSAWRITER_HPP

#include <boost/program_options.hpp>
#include <iostream>

/**
 * @struct bytesNumber
 * @brief A wrapper around a std::size_t needed by boost::program_options.
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
  std::string prefix;
  // Technically, the OutputSequence base classes, if used, does enforce
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
  bool useSequence() {
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

class msaValidator final {

  // MicrosliceDescriptor fields with custom enum classes (apart from
  // fles::Subsystem, which is commented out, because it is not used in
  // the checks).
  //
  // Note that these enum classes are not used in the code of the
  // MicrosliceDescriptor class, but they are defined in the same
  // header.)
  //
  // fles::Subsystem last_sys_id;
  fles::SubsystemFormatFLES last_sys_ver;
  fles::HeaderFormatIdentifier last_hdr_id;
  fles::HeaderFormatVersion last_hdr_ver;
  fles::MicrosliceFlags last_flags;

  // All fields of the MicrosliceDescriptor together with the comment
  // from the MicrosliceDescriptor class in the order of their
  // appearance. The ones have their own enum classes are omitted as
  // they are already listed above. Fields that currently do not need to
  // be checked are commented out.
  //
  // uint8_t hdr_id;  ///< Header format identifier (0xDD)
  // uint8_t hdr_ver; ///< Header format version (0x01)
  uint16_t last_eq_id; ///< Equipment identifier
  // uint16_t flags;  ///< Status and error flags
  // uint8_t sys_id;  ///< Subsystem identifier
  // uint8_t sys_ver; ///< Subsystem format/version
  // uint64_t idx;    ///< Microslice index / start time
  // uint32_t crc;    ///< CRC-32C (Castagnoli polynomial) of data content
  // uint32_t size;   ///< Content size (bytes)
  // uint64_t offset; ///< Offset in event buffer (bytes)

  // Statistics about their values in the order of their appearance in
  // the MicrosliceDescriptor class:

  // Change-Counters:
  uint64_t num_hdr_id_changes;
  uint64_t num_hdr_ver_changes;
  uint64_t num_eq_id_changes;
  uint64_t num_flags_changes;
  // uint64_t num_sys_id_changes; // Not used
  uint64_t num_sys_ver_changes;

  // Appeared values.
  //
  // A set is used, because only a few values are expected to appear.
  // The constant look-up/insertion time of unordered_sets likely is not
  // going to be faster than the logarithmic look-up time of sets for
  // such small sets, but likely the other way around. (Did not measure
  // it myself, though, see
  // https://stackoverflow.com/questions/1349734).
  std::set<fles::HeaderFormatIdentifier> hdr_id_seen;
  std::set<fles::HeaderFormatVersion> hdr_ver_seen;
  std::set<uint16_t> eq_id_seen;
  std::set<fles::MicrosliceFlags> flags_seen;
  std::set<fles::Subsystem> sys_id_seen;
  std::set<fles::SubsystemFormatFLES> sys_ver_seen;

  // Check if some values stay constant per Timeslice:
  std::set<fles::HeaderFormatIdentifier> hdr_id_seen_in_ts;
  std::set<fles::HeaderFormatVersion> hdr_ver_seen_in_ts;
  std::set<uint16_t> eq_id_seen_in_ts;
  std::set<fles::MicrosliceFlags> flags_seen_in_ts;
  std::set<fles::Subsystem> sys_id_seen_in_ts;
  std::set<fles::SubsystemFormatFLES> sys_ver_seen_in_ts;
  uint64_t numMicroslices_in_ts = 0;

  // Map to count the number of Equipment Identifiers appearing:
  std::map<uint16_t, uint64_t> eq_id_count;

  // Check if some values stay constant per TimesliceComponent:
  std::set<fles::HeaderFormatIdentifier> hdr_id_seen_in_tsc;
  std::set<fles::HeaderFormatVersion> hdr_ver_seen_in_tsc;
  std::set<uint16_t> eq_id_seen_in_tsc;
  std::set<fles::MicrosliceFlags> flags_seen_in_tsc;
  std::set<fles::Subsystem> sys_id_seen_in_tsc;
  std::set<fles::SubsystemFormatFLES> sys_ver_seen_in_tsc;
  uint64_t numMicroslices_in_tsc;

public:
  // Constructor:
  msaValidator()
      : last_sys_ver(static_cast<fles::SubsystemFormatFLES>(0)),
        last_hdr_id(static_cast<fles::HeaderFormatIdentifier>(0)),
        last_hdr_ver(static_cast<fles::HeaderFormatVersion>(0)),
        last_eq_id(static_cast<uint16_t>(0)),
        last_flags(static_cast<fles::MicrosliceFlags>(0)),
        // last_sys_id(static_cast<fles::Subsystem>(0)), // Not used
        last_sys_ver(static_cast<fles::SubsystemFormatFLES>(0)),

        num_hdr_id_changes(0), num_hdr_ver_changes(0), num_eq_id_changes(0),
        num_flags_changes(0),
        // num_sys_id_changes(0), // Not used
        num_sys_ver_changes(0),

        hdr_id_seen(), hdr_ver_seen(), eq_id_seen(), flags_seen(),
        sys_id_seen(), sys_ver_seen(),

        hdr_id_seen_in_ts(), hdr_ver_seen_in_ts(), eq_id_seen_in_ts(),
        flags_seen_in_ts(), sys_id_seen_in_ts(), sys_ver_seen_in_ts(),
        numMicroslices_in_ts(0),

        hdr_id_seen_in_tsc(), hdr_ver_seen_in_tsc(), eq_id_seen_in_tsc(),
        flags_seen_in_tsc(), sys_id_seen_in_tsc(), sys_ver_seen_in_tsc(),
        numMicroslices_in_tsc(0) {}

  void next_timeslice() {
    // Clear the sets for the next Timeslice:
    hdr_id_seen_in_ts.clear();
    hdr_ver_seen_in_ts.clear();
    eq_id_seen_in_ts.clear();
    flags_seen_in_ts.clear();
    sys_id_seen_in_ts.clear();
    sys_ver_seen_in_ts.clear();
    numMicroslices_in_ts = 0;
  }

  void next_timeslice_component() {
    // Clear the sets for the next TimesliceComponent:
    hdr_id_seen_in_tsc.clear();
    hdr_ver_seen_in_tsc.clear();
    eq_id_seen_in_tsc.clear();
    flags_seen_in_tsc.clear();
    sys_id_seen_in_tsc.clear();
    sys_ver_seen_in_tsc.clear();
    numMicroslices_in_tsc = 0;
  }

  void check_microslice(std::shared_ptr<fles::MicrosliceView> ms_ptr) {
    const fles::MicrosliceDescriptor& msd = ms_ptr->desc();

    // Get values from the MicrosliceDescriptor:
    // TODO: Take into account that raw values not corresponding to
    // some named enum value should be treated as an error.
    const fles::HeaderFormatIdentifier& hdr_id =
        static_cast<fles::HeaderFormatIdentifier>(msd.hdr_id);
    const fles::HeaderFormatVersion& hdr_ver =
        static_cast<fles::HeaderFormatVersion>(msd.hdr_ver);
    const uint16_t& eq_id = msd.eq_id;
    const fles::MicrosliceFlags& flags =
        static_cast<fles::MicrosliceFlags>(msd.flags);
    const fles::Subsystem& sys_id = static_cast<fles::Subsystem>(msd.sys_id);
    const fles::SubsystemFormatFLES& sys_ver =
        static_cast<fles::SubsystemFormatFLES>(msd.sys_ver);

    // Insert values into the seen sets:
    hdr_id_seen.insert(hdr_id);
    hdr_ver_seen.insert(hdr_ver);
    eq_id_seen.insert(eq_id);
    flags_seen.insert(flags);
    sys_id_seen.insert(sys_id);
    sys_ver_seen.insert(sys_ver);

    // Insert values into the seen sets of the Timeslice:
    hdr_id_seen_in_ts.insert(hdr_id);
    hdr_ver_seen_in_ts.insert(hdr_ver);
    eq_id_seen_in_ts.insert(eq_id);
    flags_seen_in_ts.insert(flags);
    sys_id_seen_in_ts.insert(sys_id);
    sys_ver_seen_in_ts.insert(sys_ver);

    // Insert values into the seen sets of the TimesliceComponent:
    hdr_id_seen_in_tsc.insert(hdr_id);
    hdr_ver_seen_in_tsc.insert(hdr_ver);
    auto tsc_insertion_result_eq_id = eq_id_seen_in_tsc.insert(eq_id);
    flags_seen_in_tsc.insert(flags);
    sys_id_seen_in_tsc.insert(sys_id);
    sys_ver_seen_in_tsc.insert(sys_ver);

    if (tsc_insertion_result_eq_id.second) {
      // If the Equipment Identifier is new, insert it into the
      // count map with a count of 1. This effectively counts the
      // number of Equipment Identifiers appearing in the
      // TimesliceComponent.
      //
      // Note: According to the C++ standard, the container
      // zero-initializes the value if the key is not found (or for
      // class types, the default constructor is called). This is
      // why it is sufficient to simply increment the value here.
      eq_id_count[eq_id]++;
    }

    // Compare to last values:
    if (numMicroslices > 0) {
      if (hdr_id != last_hdr_id) {
        ++num_hdr_id_changes;
      }
      if (hdr_ver != last_hdr_ver) {
        ++num_hdr_ver_changes;
      }
      if (eq_id != last_eq_id) {
        ++num_eq_id_changes;
      }
      if (flags != last_flags) {
        ++num_flags_changes;
      }
      // if (sys_id != last_sys_id) { // Not used
      //  ++num_sys_id_changes;
      //}
      if (sys_ver != last_sys_ver) {
        ++num_sys_ver_changes;
      }
    }
    numMicroslices++;
    numMicroslices_in_ts++;
    numMicroslices_in_tsc++;

    // Set last values
    last_hdr_id = static_cast<fles::HeaderFormatIdentifier>(msd.hdr_id);
    last_hdr_ver = static_cast<fles::HeaderFormatVersion>(msd.hdr_ver);
    last_eq_id = msd.eq_id;
    last_flags = static_cast<fles::MicrosliceFlags>(msd.flags);
    // last_sysId = static_cast<fles::Subsystem>(msd.sys_id); // Not used
    last_sys_ver = static_cast<fles::SubsystemFormatFLES>(msd.sys_ver);
  }

  void print_timeslice_info() {
    std::cout << "Timeslice " << numTimeslices << ":" << std::endl;
    std::cout << "Microslice in ts " << numMicroslices_in_ts << std::endl;

    // TODO: In all of the following, the human-readable names of
    // the enum values should be printed instead of the raw
    // values, unless the raw value does not have a human-readable
    // name, which should be treated as an error.
    //
    // Note: The fixed width integer types may be (and often are)
    // implemented as typedefs to (unsigned) chars or similar,
    // leading to strange output. Hence, we must cast them to a
    // different integer type before printing them. See:
    // https://stackoverflow.com/questions/30447201

    // Print current hdr_id, set of appeared hdr_ids and its size,
    // and the number of changes of hdr_id, all in one line:
    std::cout << "num hdr_id: " << hdr_id_seen_in_ts.size() << " seen: ";
    for (const auto& seen_hdr_id : hdr_id_seen_in_ts) {
      std::cout << static_cast<int>(seen_hdr_id) << " ";
    }
    std::cout << std::endl;

    // Print current hdr_ver, set of appeared hdr_vers and its size,
    // and the number of changes of hdr_ver, all in one line:
    std::cout << "num hdr_ver: " << hdr_ver_seen_in_ts.size() << " seen: ";
    for (const auto& seen_hdr_ver : hdr_ver_seen_in_ts) {
      std::cout << static_cast<int>(seen_hdr_ver) << " ";
    }
    std::cout << std::endl;

    // Print current eq_id, set of appeared eq_ids and its size,
    // and the number of changes of eq_id, all in one line:
    // Note: No static_cast is needed here, because eq_id does not
    // have a custom enum class.
    std::cout << "num eq_id: " << eq_id_seen_in_ts.size() << " seen: ";
    for (const auto& seen_eq_id : eq_id_seen_in_ts) {
      std::cout << static_cast<int>(seen_eq_id) << " ";
      std::cout << "(" << eq_id_count[seen_eq_id] << " per ts) ";
    }
    std::cout << std::endl;

    // Print current flags, set of appeared flags and its size,
    // and the number of changes of flags, all in one line:
    // TODO: These are likely or-ed together, so we should rewrite
    // the code to take that into account.
    std::cout << "num flags: " << flags_seen_in_ts.size() << " seen: ";
    for (const auto& seen_flags : flags_seen_in_ts) {
      std::cout << static_cast<int>(seen_flags) << " ";
    }
    std::cout << std::endl;

    // Print current sys_id, set of appeared sys_ids and its size,
    // and the number of changes of sys_id, all in one line:
    // TODO: No number of changes is printed, but printing the
    // number of sys_ids per Timeslice and TimesliceComponent might
    // be useful.
    std::cout << "num sys_id: " << sys_id_seen_in_ts.size() << " seen: ";
    for (const auto& seen_sys_id : sys_id_seen_in_ts) {
      std::cout << static_cast<int>(seen_sys_id) << " ";
    }
    std::cout << std::endl;

    // Print current sys_ver, set of appeared sys_vers and its size,
    // and the number of changes of sys_ver, all in one line:
    std::cout << "num sys_ver: " << sys_ver_seen_in_ts.size() << " seen: ";
    for (const auto& seen_sys_ver : sys_ver_seen_in_ts) {
      std::cout << static_cast<int>(seen_sys_ver) << " ";
    }
    std::cout << std::endl;

    std::cout << std::endl; // Empty line for better readability
  }

  void print_timeslice_component_info() {
    std::cout << "TimesliceComponent " << tsc << " in ts " << numTimeslices
              << ":" << std::endl;
    std::cout << "Microslice in tsc " << numMicroslices_in_tsc << std::endl;

    // TODO: In all of the following, the human-readable names of
    // the enum values should be printed instead of the raw
    // values, unless the raw value does not have a human-readable
    // name, which should be treated as an error.
    //
    // Note: The fixed width integer types may be (and often are)
    // implemented as typedefs to (unsigned) chars or similar,
    // leading to strange output. Hence, we must cast them to a
    // different integer type before printing them. See:
    // https://stackoverflow.com/questions/30447201

    // Print current hdr_id, set of appeared hdr_ids and its size,
    // and the number of changes of hdr_id, all in one line:
    std::cout << "num hdr_id: " << hdr_id_seen_in_tsc.size() << " seen: ";
    for (const auto& seen_hdr_id : hdr_id_seen_in_tsc) {
      std::cout << static_cast<int>(seen_hdr_id) << " ";
    }
    std::cout << std::endl;

    // Print current hdr_ver, set of appeared hdr_vers and its size,
    // and the number of changes of hdr_ver, all in one line:
    std::cout << "num hdr_ver: " << hdr_ver_seen_in_tsc.size() << " seen: ";
    for (const auto& seen_hdr_ver : hdr_ver_seen_in_tsc) {
      std::cout << static_cast<int>(seen_hdr_ver) << " ";
    }
    std::cout << std::endl;

    // Print current eq_id, set of appeared eq_ids and its size,
    // and the number of changes of eq_id, all in one line:
    // Note: No static_cast is needed here, because eq_id does not
    // have a custom enum class.
    std::cout << "num eq_id: " << eq_id_seen_in_tsc.size() << " seen: ";
    for (const auto& seen_eq_id : eq_id_seen_in_tsc) {
      std::cout << static_cast<int>(seen_eq_id) << " ";
    }
    std::cout << std::endl;

    // Print current flags, set of appeared flags and its size,
    // and the number of changes of flags, all in one line:
    // TODO: These are likely or-ed together, so we should rewrite
    // the code to take that into account.
    std::cout << "num flags: " << flags_seen_in_tsc.size() << " seen: ";
    for (const auto& seen_flags : flags_seen_in_tsc) {
      std::cout << static_cast<int>(seen_flags) << " ";
    }
    std::cout << std::endl;

    // Print current sys_id, set of appeared sys_ids and its size,
    // and the number of changes of sys_id, all in one line:
    // TODO: No number of changes is printed, but printing the
    // number of sys_ids per Timeslice and TimesliceComponent might
    // be useful.
    std::cout << "num sys_id: " << sys_id_seen_in_tsc.size() << " seen: ";
    for (const auto& seen_sys_id : sys_id_seen_in_tsc) {
      std::cout << static_cast<int>(seen_sys_id) << " ";
    }
    std::cout << std::endl;

    // Print current sys_ver, set of appeared sys_vers and its size,
    // and the number of changes of sys_ver, all in one line:
    std::cout << "num sys_ver: " << sys_ver_seen_in_tsc.size() << " seen: ";
    for (const auto& seen_sys_ver : sys_ver_seen_in_tsc) {
      std::cout << static_cast<int>(seen_sys_ver) << " ";
    }
    std::cout << std::endl;

    std::cout << std::endl; // Empty line for better readability
  }

  void print_microslice_stats() {
    std::cout << "Microslice " << numMicroslices << ":" << std::endl;

    // TODO: In all of the following, the human-readable names of
    // the enum values should be printed instead of the raw
    // values, unless the raw value does not have a human-readable
    // name, which should be treated as an error.
    //
    // Note: The fixed width integer types may be (and often are)
    // implemented as typedefs to (unsigned) chars or similar,
    // leading to strange output. Hence, we must cast them to a
    // different integer type before printing them. See:
    // https://stackoverflow.com/questions/30447201

    // Print current hdr_id, set of appeared hdr_ids and its size,
    // and the number of changes of hdr_id, all in one line:
    std::cout << "hdr_id: " << static_cast<int>(hdr_id) << " ("
              << hdr_id_seen.size() << " seen: ";
    for (const auto& seen_hdr_id : hdr_id_seen) {
      std::cout << static_cast<int>(seen_hdr_id) << " ";
    }
    std::cout << ") changes: " << num_hdr_id_changes << std::endl;

    // Print current hdr_ver, set of appeared hdr_vers and its size,
    // and the number of changes of hdr_ver, all in one line:
    std::cout << "hdr_ver: " << static_cast<int>(hdr_ver) << " ("
              << hdr_ver_seen.size() << " seen: ";
    for (const auto& seen_hdr_ver : hdr_ver_seen) {
      std::cout << static_cast<int>(seen_hdr_ver) << " ";
    }
    std::cout << ") changes: " << num_hdr_ver_changes << std::endl;

    // Print current eq_id, set of appeared eq_ids and its size,
    // and the number of changes of eq_id, all in one line:
    // Note: No static_cast is needed here, because eq_id does not
    // have a custom enum class.
    std::cout << "eq_id: " << static_cast<int>(eq_id) << " ("
              << eq_id_seen.size() << " seen: ";
    for (const auto& seen_eq_id : eq_id_seen) {
      std::cout << static_cast<int>(seen_eq_id) << " ";
    }
    std::cout << ") changes: " << num_eq_id_changes << std::endl;

    // Print current flags, set of appeared flags and its size,
    // and the number of changes of flags, all in one line:
    // TODO: These are likely or-ed together, so we should rewrite
    // the code to take that into account.
    std::cout << "flags: " << static_cast<int>(flags) << " ("
              << flags_seen.size() << " seen: ";
    for (const auto& seen_flags : flags_seen) {
      std::cout << static_cast<int>(seen_flags) << " ";
    }
    std::cout << ") changes: " << num_flags_changes << std::endl;

    // Print current sys_id, set of appeared sys_ids and its size,
    // and the number of changes of sys_id, all in one line:
    // TODO: No number of changes is printed, but printing the
    // number of sys_ids per Timeslice and TimesliceComponent might
    // be useful.
    std::cout << "sys_id: " << static_cast<int>(sys_id) << " ("
              << sys_id_seen.size() << " seen: ";
    for (const auto& seen_sys_id : sys_id_seen) {
      std::cout << static_cast<int>(seen_sys_id) << " ";
    }
    std::cout << ") " << std::endl;

    // Print current sys_ver, set of appeared sys_vers and its size,
    // and the number of changes of sys_ver, all in one line:
    std::cout << "sys_ver: " << static_cast<int>(sys_ver) << " ("
              << sys_ver_seen.size() << " seen: ";
    for (const auto& seen_sys_ver : sys_ver_seen) {
      std::cout << static_cast<int>(seen_sys_ver) << " ";
    }
    std::cout << ") changes: " << num_sys_ver_changes << std::endl;

    std::cout << std::endl; // Empty line for better readability
  }
};

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

  void write_timeslice() {
    // TODO: Count sys_id changes per timeslice, check if there is new
    // or missing sys_ids.

    // Inform the validator that a new Timeslice is being written:
    validator.next_timeslice();

    // Write the timeslice to a file:
    for (uint64_t tsc = 0; tsc < timeslice->num_components(); tsc++) {
      msaWriter.write_timeslice_component();
    }

    if (beVerbose) {
      validator.print_timeslice_info();
    }

    if (tsaReaderOptions.interactive) {
      std::cout << "Press Enter to continue..." << std::endl;
      std::cin.get();
    }
    numTimeslices++;
  }

  void write_timeslice_component() {
    // TODO: Count sys_id changes per TimesliceComponent, check if
    // there is new or missing sys_ids.

    // Inform the validator that a new TimesliceComponent is being written:
    validator.next_timeslice_component();

    for (uint64_t msc = 0; msc < timeslice->num_core_microslices(tsc); msc++) {
      std::unique_ptr<fles::MicrosliceView> ms_ptr =
          std::make_unique<fles::MicrosliceView>(
              timeslice->get_microslice(tsc, msc));
      msaWriter.write_microslice(std::move(ms_ptr));
    }

    if (false && beVerbose) {
      validator.print_timeslice_component_info();
    }

    if (false && tsaReaderOptions.interactive) {
      std::cout << "Press Enter to continue..." << std::endl;
      std::cin.get();
    }
  }

  std::string constructArchiveName() {
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
        msaWriterOptions.useSequence() ? "_%n" : "";

    std::string msa_archive_name = prefix + "_" + sys_id_string + "_" +
                                   eq_id_string + optionalSequenceIndicator +
                                   ".msa";
    // TODO: This happens for every microslice, which is really
    // unnecessary and should happen for every TimesliceComponent
    // instead. Only that the values stay constant per
    // TimesliceComponent needs to be checked here.
    if (msaFiles.find(msa_archive_name) == msaFiles.end()) {
      std::unique_ptr<fles::Sink<fles::Microslice>> msaFile;
      if (msaWriterOptions.useSequence()) {
        msaFile = std::make_unique<fles::MicrosliceOutputArchiveSequence>(
            msa_archive_name, msaWriterOptions.maxItemsPerArchive,
            msaWriterOptions.maxBytesPerArchive);
      } else {
        msaFile =
            std::make_unique<fles::MicrosliceOutputArchive>(msa_archive_name);
      }
      msaFiles[msa_archive_name] = std::move(msaFile);
    }
  }

  void write_microslice(std::shared_ptr<fles::MicrosliceView> ms_ptr) {
    const fles::MicrosliceDescriptor& msd = ms_ptr->desc();

    // Get values from the MicrosliceDescriptor:
    // TODO: Take into account that raw values not corresponding to
    // some named enum value should be treated as an error.
    const fles::HeaderFormatIdentifier& hdr_id =
        static_cast<fles::HeaderFormatIdentifier>(msd.hdr_id);
    const fles::HeaderFormatVersion& hdr_ver =
        static_cast<fles::HeaderFormatVersion>(msd.hdr_ver);
    const uint16_t& eq_id = msd.eq_id;
    const fles::MicrosliceFlags& flags =
        static_cast<fles::MicrosliceFlags>(msd.flags);
    const fles::Subsystem& sys_id = static_cast<fles::Subsystem>(msd.sys_id);
    const fles::SubsystemFormatFLES& sys_ver =
        static_cast<fles::SubsystemFormatFLES>(msd.sys_ver);
    if (false && beVerbose) {
      validator.print_microslice_stats();
    }

    if (!msaWriterOptions.dryRun) {
      std::string msa_archive_name = constructArchiveName();
      msaFiles[msa_archive_name]->put(std::move(ms_ptr));
    }

    if (false && tsaReaderOptions.interactive) {
      std::cout << "Press Enter to continue..." << std::endl;
      std::cin.get();
    }
  }

private:
  const msaWriterOptions options;
};

#endif // MSAWRITER_HPP
