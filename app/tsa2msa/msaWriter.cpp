#include "msaWriter.hpp"
#include "utils.hpp"

#include <boost/program_options.hpp>

#include <iostream>

msaWriter::msaWriter(const msaWriterOptions& options) : options(options) {
  // Do nothing for now
}

msaWriterOptions::msaWriterOptions()
    : // clang-format off
    // [msaWriterDefaults]
    dryRun(false),
    beVerbose(false),
    interactive(false),
    prefix(""),
    maxItemsPerArchive(0), // 0 means no limit
    maxBytesPerArchive(0)  // 0 means no limit
    // [msaWriterDefaults]
      // clang-format on
      {};

boost::program_options::options_description
msaWriterOptions::optionsDescription(bool hidden) {
  /*
   * Note: boost::program_options throws a runtime exception if multiple
   * options_description objects which contain the same option are
   * combined. Hence, name clashes must be avoided manually.
   */
  if (hidden) {
    boost::program_options::options_description desc(
        "Additional MSA Writer Options");
    return desc; // No hidden options for now
  } else {
    boost::program_options::options_description desc("MSA Writer Options");
    // clang-format off
    desc.add_options()
        ("dry-run,d",
          boost::program_options::bool_switch(&this->dryRun)
              -> default_value(this->dryRun),
          "Dry run (do not write any msa files)") 
        ("prefix,p",
          boost::program_options::value<std::string>(&this->prefix)
              -> default_value(this->prefix),
          "Output prefix for msa files\n"
          "  \tIf no prefix is given, i.e. the value default to the empty"
          " string, the longest common prefix of the input files is used."
          )
        ("max-items",
          boost::program_options::value<std::size_t>(&this->maxItemsPerArchive)
              -> default_value(this->maxItemsPerArchive),
          "Maximum number of items to write to msa files")
        ("max-size",
          boost::program_options::value<bytesNumber>(&this->maxBytesPerArchive)
            -> default_value(this->maxBytesPerArchive),
          "Maximum size of msa files:\n"
          "  \tHuman readable units according to the SI or IEC standart (e.g"
          " 1kB = 1000B according to SI, 1KiB = 1024B according to IEC) are"
          " supported, zero means no limit (default).\n"
          "\n"
          "  \tUnits are case"
          " insensitive and default unit is bytes. Both, using a space"
          " between number and unit or not, are supported. (Make sure that"
          " your shell does not interpret the space as a separator between"
          " arguments.)\n")
        ;
    // clang-formBytesPerArchiven
    return desc;
  }
}

void msaWriter::write(std::shared_ptr<fles::Timeslice> timeslice) {
    // TODO: Count sys_id changes per timeslice, check if there is new
    // or missing sys_ids.


    // Write the timeslice to a file:
    for (uint64_t tsc = 0; tsc < timeslice->num_components(); tsc++) {
      write(timeslice, tsc);
    }

    // Inform the validator that the Timeslice has ended:
    validator.timesliceEnd(options.beVerbose);

    if (options.interactive) {
      std::cout << "Press Enter to continue..." << std::endl;
      std::cin.get();
    }
    numTimeslices++;
  }

  void msaWriter::write(std::shared_ptr<fles::Timeslice> timeslice,
                                 uint64_t tsc) {
    // TODO: Count sys_id changes per TimesliceComponent, check if
    // there is new or missing sys_ids.

    // TODO: Check for num_core_microslices() changes.
    // TODO: Check if microslices get lost at first resp. last component
    for (uint64_t msc = 0; msc < timeslice->num_core_microslices(); msc++) {
      std::unique_ptr<fles::MicrosliceView> ms_ptr =
          std::make_unique<fles::MicrosliceView>(
              timeslice->get_microslice(tsc, msc));
      write(std::move(ms_ptr));
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

  void msaWriter::write(std::shared_ptr<fles::MicrosliceView> ms_ptr) {
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
