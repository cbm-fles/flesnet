#include "msaWriter.hpp"

#include <boost/program_options.hpp>

#include <iostream>

msaWriter::msaWriter(const msaWriterOptions& options) : options(options) {
  // Do nothing for now
}

std::ostream& operator<<(std::ostream& os, const struct bytesNumber& bN) {
  return os << bN.value;
}

msaWriterOptions defaultMsaWriterOptions() {
  // Make sure to update doxygen documentation at declaration site if
  // the default values are changed.
  return {
      false, // dryRun  // TODO: make .dryRun = false syntax work
      false, // beVerbose
      false, // interactive
      "",    // prefix
      0,     // maxItems
      0      // maxSize
  };
}

boost::program_options::options_description
getMsaWriterOptionsDescription(msaWriterOptions& options, bool hidden) {
  if (hidden) {
    boost::program_options::options_description desc(
        "Additional MSA Writer Options");
    return desc; // No hidden options for now
  } else {
    boost::program_options::options_description desc("MSA Writer Options");
    // clang-format off
    desc.add_options()
        ("dry-run,d",
          boost::program_options::bool_switch(&options.dryRun),
          "Dry run (do not write any msa files)") 
        ("prefix,p",
          boost::program_options::value<std::string>(&options.prefix)
              -> default_value(""),
          "Output prefix for msa files\n"
          "  \tIf no prefix is given, i.e. the value default to the empty"
          " string, the longest common prefix of the input files is used."
          )
        ("max-items",
          boost::program_options::value<std::size_t>(&options.maxItemsPerArchive)
              -> default_value(0),
          "Maximum number of items to write to msa files")
        ("max-size",
          boost::program_options::value<bytesNumber>(&options.maxBytesPerArchive)
            -> default_value(bytesNumber{0}),
          // TODO: The following text is too long for a --help message
          // and should be placed somewhere else.
          "Maximum size of msa files:\n"
          "  \tHuman readable units according to the SI or IEC standart are"
          " supported, zero means no limit (default). Units are case"
          " insensitive and default unit is bytes. Both, using a space"
          " between number and unit or not, are supported. (Make sure that"
          " your shell does not interpret the space as a separator between"
          " arguments.)\n")
        ;
    // clang-formBytesPerArchiven
    return desc;
  }
}

void validate(boost::any& v, const std::vector<std::string>& values,
              bytesNumber*, int) {
  // Following the example in the boost program_options documentation
  // (e.g. boost version 1.85.0) for validating custom types, the
  // following code needs to be added here before the input can be
  // validated (simply copied from the example):

  // Make sure no previous assignment to 'v' was made.
  boost::program_options::validators::check_first_occurrence(v);

  // Extract the first string from 'values'. If there is more than
  // one string, it's an error, and exception will be thrown.
  const std::string& s = boost::program_options::validators::get_single_string(values);

  // Split s in number and unit:
  std::size_t number;
  std::string unit; 
  std::istringstream is(s);
  is >> number;
  if (is.fail()) {
    std::cerr << "Invalid value: " << s << ", see --help for available options" << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  }

  if (is.eof()) {
    // No unit given, default to bytes
    unit = "B";
  } else {
    // Read the unit
    is >> unit;
  }

  if (is.fail()) {
    std::cerr << "Invalid unit after parsing number: " << number << "input: " << s << ", see --help for available options" << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  }

  if (!is.eof()) {
    // Check if there is unnecessary whitespace at the end of the input:
    std::string rest;
    is >> rest;
    for (auto& c : rest) {
      if (!std::isspace(c)) {
        std::cerr << "Error: invalid character after parsing unit: " << c << std::endl;
        throw boost::program_options::validation_error(
            boost::program_options::validation_error::invalid_option_value);
      }
    }

    // If all characters are whitespace, the input is valid. Ignore the
    // whitespace.
  }

  // Check if the unit is valid:
  std::map<std::string, std::size_t> specialUnits = { {"",1} };

  // decimal units according to the SI standard
  std::map<std::string, std::size_t> SI = {
      {"B", 1},
      {"kB", 1000},
      {"MB", 1000 * 1000},
      {"GB", 1000 * 1000 * 1000},
      // Larger units result in overflow for std::size_t and hence
      // cannot be used:
      // {"tb", 1000 * 1000 * 1000 * 1000},
      // {"pb", 1000 * 1000 * 1000 * 1000 * 1000},
      // {"eb", 1000 * 1000 * 1000 * 1000 * 1000 * 1000},
      // {"zb", 1000 * 1000 * 1000 * 1000 * 1000 * 1000 * 1000},
      // {"yb", 1000 * 1000 * 1000 * 1000 * 1000 * 1000 * 1000 * 1000},
  };

  // binary units according to the IEC standard:
  std::map<std::string, std::size_t> IEC = {
      {"B", 1},
      {"KiB", 1024},
      {"MiB", 1024 * 1024},
      {"GiB", 1024 * 1024 * 1024},
      // Larger units result in overflow for std::size_t and hence
      // cannot be used:
      // {"TiB", 1024 * 1024 * 1024 * 1024},
      // {"PiB", 1024 * 1024 * 1024 * 1024 * 1024},
      // {"EiB", 1024 * 1024 * 1024 * 1024 * 1024 * 1024},
      // {"ZiB", 1024 * 1024 * 1024 * 1024 * 1024 * 1024 * 1024},
      // {"YiB", 1024 * 1024 * 1024 * 1024 * 1024 * 1024 * 1024 * 1024},
  };

  // Join the maps:
  std::map<std::string, std::size_t> units;
  units.insert(specialUnits.begin(), specialUnits.end());
  units.insert(SI.begin(), SI.end());
  units.insert(IEC.begin(), IEC.end());

  // Check if the unit is valid (ignoring case):
  std::size_t factor = 0; // zero until a valid unit is found
  for (auto& c : units) {
    if (c.first.size() != unit.size()) {
      // This check is necessary to prevent the special empty unit from
      // breaking the comparison performed in the next loop.
      continue;
    }

    bool found = true; // until proven otherwise
    for (std::size_t i = 0; i < c.first.size() && i < unit.size(); ++i) {
      if (std::tolower(c.first[i]) != std::tolower(unit[i])) {
        found = false;
        break;
      }
    }
    if (found) {
      factor = c.second;
      break;
    }
  }

  if (factor == 0) {
    std::cerr << "Invalid unit: " << unit << ", see --help for available options" << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  }

  // Assign the value to 'v': 
  v = boost::any(bytesNumber{number * factor});

}

void msaWriter::write_timeslice(std::shared_ptr<fles::Timeslice> timeslice) {
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

  void msaWriter::write_timeslice_component(std::shared_ptr<fles::Timeslice> timeslice,
                                 uint64_t tsc) {
    // TODO: Count sys_id changes per TimesliceComponent, check if
    // there is new or missing sys_ids.

    // TODO: Check for num_core_microslices() changes.
    // TODO: Check if microslices get lost at first resp. last component
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

  std::string msaWriter::constructArchiveName(const fles::Subsystem& sys_id,
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

  void msaWriter::write_microslice(std::shared_ptr<fles::MicrosliceView> ms_ptr) {
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
