
#include <iostream>
#include <set>

// FLESnet Library header files:
#include "lib/fles_ipc/MicrosliceOutputArchive.hpp"
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

#include "msaValidator.hpp"

msaValidator::msaValidator()
      : numTimeslices(0),
        tsc(0),

        last_sys_id(static_cast<fles::Subsystem>(0)),
        last_sys_ver(static_cast<fles::SubsystemFormatFLES>(0)),
        last_hdr_id(static_cast<fles::HeaderFormatIdentifier>(0)),
        last_hdr_ver(static_cast<fles::HeaderFormatVersion>(0)),
        last_flags(static_cast<fles::MicrosliceFlags>(0)),

        last_eq_id(0),

        num_hdr_id_changes(0), num_hdr_ver_changes(0), num_eq_id_changes(0),
        num_flags_changes(0),
        num_sys_id_changes(0),
        num_sys_ver_changes(0),

        hdr_id_seen(), hdr_ver_seen(), eq_id_seen(), flags_seen(),
        sys_id_seen(), sys_ver_seen(),

        hdr_id_seen_in_ts(), hdr_ver_seen_in_ts(), eq_id_seen_in_ts(),
        flags_seen_in_ts(), sys_id_seen_in_ts(), sys_ver_seen_in_ts(),
        numMicroslices_in_ts(0),

        hdr_id_seen_in_tsc(), hdr_ver_seen_in_tsc(), eq_id_seen_in_tsc(),
        flags_seen_in_tsc(), sys_id_seen_in_tsc(), sys_ver_seen_in_tsc(),
        numMicroslices_in_tsc(0) {}

void msaValidator::timesliceEnd(const bool& printInfo) {
    // Obvioudly, it is crucial to print the info about the timeslice
    // before clearing the corresponding state variables.
    if (printInfo) {
      print_timeslice_info();
    }

    // Clear the sets for the next Timeslice:
    hdr_id_seen_in_ts.clear();
    hdr_ver_seen_in_ts.clear();
    eq_id_seen_in_ts.clear();
    flags_seen_in_ts.clear();
    sys_id_seen_in_ts.clear();
    sys_ver_seen_in_ts.clear();
    numMicroslices_in_ts = 0;
  }

void msaValidator::timesliceComponentEnd(const bool& printInfo) {
    // Obvioudly, it is crucial to print the info about the timeslice
    // component before clearing the corresponding state variables.
    if (printInfo) {
      print_timeslice_component_info();
    }

    // Clear the sets for the next TimesliceComponent:
    hdr_id_seen_in_tsc.clear();
    hdr_ver_seen_in_tsc.clear();
    eq_id_seen_in_tsc.clear();
    flags_seen_in_tsc.clear();
    sys_id_seen_in_tsc.clear();
    sys_ver_seen_in_tsc.clear();
    numMicroslices_in_tsc = 0;
  }

  void msaValidator::insert_values_into_seen_sets(const fles::HeaderFormatIdentifier& hdr_id,
                                    const fles::HeaderFormatVersion& hdr_ver,
                                    const uint16_t& eq_id,
                                    const fles::MicrosliceFlags& flags,
                                    const fles::Subsystem& sys_id,
                                    const fles::SubsystemFormatFLES& sys_ver) {

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
  }

  void msaValidator::check_against_last_values(const fles::HeaderFormatIdentifier& hdr_id,
                                 const fles::HeaderFormatVersion& hdr_ver,
                                 const uint16_t& eq_id,
                                 const fles::MicrosliceFlags& flags,
                                 const fles::Subsystem& sys_id,
                                 const fles::SubsystemFormatFLES& sys_ver) {

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
      if (sys_id != last_sys_id) { // Not used
        ++num_sys_id_changes;
      }
      if (sys_ver != last_sys_ver) {
        ++num_sys_ver_changes;
      }
    }
    numMicroslices++;
    numMicroslices_in_ts++;
    numMicroslices_in_tsc++;

    // Set last values
    last_hdr_id = static_cast<fles::HeaderFormatIdentifier>(hdr_id);
    last_hdr_ver = static_cast<fles::HeaderFormatVersion>(hdr_ver);
    last_eq_id = eq_id;
    last_flags = static_cast<fles::MicrosliceFlags>(flags);
    last_sys_id = static_cast<fles::Subsystem>(sys_id);
    last_sys_ver = static_cast<fles::SubsystemFormatFLES>(sys_ver);

  }

  void msaValidator::check_microslice(std::shared_ptr<fles::MicrosliceView> ms_ptr, const bool& printInfo) {
    const fles::MicrosliceDescriptor& msd = ms_ptr->desc();

    // Get values from the MicrosliceDescriptor:
    // TODO: Take into account that raw values not corresponding to
    // some named enum value should be treated as an error.
    // TODO: It should be evaluated whether it makes sense to define a
    // struct that contains those values instead of handling them as
    // separate variables.
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

    insert_values_into_seen_sets(hdr_id, hdr_ver, eq_id, flags, sys_id, sys_ver);
    check_against_last_values(hdr_id, hdr_ver, eq_id, flags, sys_id, sys_ver);

    if (printInfo) {
      print_microslice_stats();
    }
  }

  void msaValidator::print_timeslice_info() {
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

  void msaValidator::print_timeslice_component_info() {
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

  void msaValidator::print_microslice_stats() {
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
    std::cout << "hdr_id: " << static_cast<int>(last_hdr_id) << " ("
              << hdr_id_seen.size() << " seen: ";
    for (const auto& seen_hdr_id : hdr_id_seen) {
      std::cout << static_cast<int>(seen_hdr_id) << " ";
    }
    std::cout << ") changes: " << num_hdr_id_changes << std::endl;

    // Print current hdr_ver, set of appeared hdr_vers and its size,
    // and the number of changes of hdr_ver, all in one line:
    std::cout << "hdr_ver: " << static_cast<int>(last_hdr_ver) << " ("
              << hdr_ver_seen.size() << " seen: ";
    for (const auto& seen_hdr_ver : hdr_ver_seen) {
      std::cout << static_cast<int>(seen_hdr_ver) << " ";
    }
    std::cout << ") changes: " << num_hdr_ver_changes << std::endl;

    // Print current eq_id, set of appeared eq_ids and its size,
    // and the number of changes of eq_id, all in one line:
    // Note: No static_cast is needed here, because eq_id does not
    // have a custom enum class.
    std::cout << "eq_id: " << static_cast<int>(last_eq_id) << " ("
              << eq_id_seen.size() << " seen: ";
    for (const auto& seen_eq_id : eq_id_seen) {
      std::cout << static_cast<int>(seen_eq_id) << " ";
    }
    std::cout << ") changes: " << num_eq_id_changes << std::endl;

    // Print current flags, set of appeared flags and its size,
    // and the number of changes of flags, all in one line:
    // TODO: These are likely or-ed together, so we should rewrite
    // the code to take that into account.
    std::cout << "flags: " << static_cast<int>(last_flags) << " ("
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
    std::cout << "sys_id: " << static_cast<int>(last_sys_id) << " ("
              << sys_id_seen.size() << " seen: ";
    for (const auto& seen_sys_id : sys_id_seen) {
      std::cout << static_cast<int>(seen_sys_id) << " ";
    }
    std::cout << ") " << std::endl;

    // Print current sys_ver, set of appeared sys_vers and its size,
    // and the number of changes of sys_ver, all in one line:
    std::cout << "sys_ver: " << static_cast<int>(last_sys_ver) << " ("
              << sys_ver_seen.size() << " seen: ";
    for (const auto& seen_sys_ver : sys_ver_seen) {
      std::cout << static_cast<int>(seen_sys_ver) << " ";
    }
    std::cout << ") changes: " << num_sys_ver_changes << std::endl;

    std::cout << std::endl; // Empty line for better readability
  }
