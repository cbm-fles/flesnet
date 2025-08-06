#ifndef MSAVALIDATOR_HPP
#define MSAVALIDATOR_HPP

#include <iostream>
#include <set>

// Flesnet Library header files:
#include "lib/fles_ipc/MicrosliceOutputArchive.hpp"
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

/**
 * @file msaValidator.hpp
 * @brief Header file for the msaValidator class used by the msaWriter.
 */

/**
 * @brief Class to validate the MicrosliceDescriptors of a Timeslice.
 * @details The class is designed to check the values appearing in the
 * microslice descriptors. It checks consistency of the values within a
 * timeslice components, timeslices and consistency overall, as well as
 * erroneous microslice descriptors.
 *
 * \todo Apparently, I forgot to implement the most important check for
 * the chronological order of the microslice descriptors. I thought I
 * did, because I manually checked their order so often.
 * \todo Implement the CRC-32C check.
 */
class msaValidator final {
  uint64_t numTimeslices;

  // Within the current Timeslice, the current TimesliceComponent
  // number:
  uint64_t tsc;

  // TODO: Make a template class for everything that is needed for
  // checking an individual field of the MicrosliceDescriptor.

  // MicrosliceDescriptor fields with custom enum classes (apart from
  // fles::Subsystem, which is commented out, because it is not used in
  // the checks).
  //
  // Note that these enum classes are not used in the code of the
  // MicrosliceDescriptor class, but they are defined in the same
  // header.)
  //
  fles::Subsystem last_sys_id;
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
  uint16_t last_eq_id; ///< Equipment identifier
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
  uint64_t num_sys_id_changes;
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
  uint64_t numMicroslices;

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
  msaValidator();

  /**
   * @brief Signal the end of a Timeslice.
   * @details The validator checks consistency of the values within a
   * timeslice, and hence, it must be informed about the end of a
   * timeslice. Otherwise, it reports false errors.
   *
   * @param printInfo If true, print the statistics of the Timeslice.
   *
   */
  void timesliceEnd(const bool& printInfo);

  /**
   * @brief Signal the end of a TimesliceComponent.
   * @details The validator checks consistency of the values within a
   * timeslice component, and hence, it must be informed about the end of
   * a timeslice component. Otherwise, it reports false errors.
   *
   * @param printInfo If true, print the statistics of the TimesliceComponent.
   */
  void timesliceComponentEnd(const bool& printInfo);

  /**
   * @brief Check the given Microslice.
   *
   * \pre After a timeslice or timeslice component has been processed,
   * timesliceEnd() resp. timesliceComponentEnd() must be called before
   * microslices of the next timeslice resp. timeslice component can be
   * checked. Otherwise, the results of the checks are not meaningful.
   *
   * \todo Currently, information is just gathered and optionally
   * printed. In the future, the function should be extended to return
   * information about occurring errors (immediately) such that the
   * caller can react to them in a meaningful way.
   *
   * @param ms_ptr Pointer to the Microslice to be checked.
   */
  void check_microslice(std::shared_ptr<fles::MicrosliceView> ms_ptr,
                        const bool& printInfo);

private:
  /**
   * @brief Print statistics about the current Timeslice.
   * @note This function could be called at any time, but it makes sense
   * to call it at the end of a Timeslice.
   */
  void print_timeslice_info();

  /**
   * @brief Print statistics about the current TimesliceComponent.
   * @note This function could be called at any time, but it makes sense
   * to call it at the end of a TimesliceComponent.
   */
  void print_timeslice_component_info();

  /**
   * @brief Print statistics about the Microslices.
   * @note This function not only prints information about the current
   * microslice, but also about the microslices seen so far.
   */
  void print_microslice_stats();

  void insert_values_into_seen_sets(const fles::HeaderFormatIdentifier& hdr_id,
                                    const fles::HeaderFormatVersion& hdr_ver,
                                    const uint16_t& eq_id,
                                    const fles::MicrosliceFlags& flags,
                                    const fles::Subsystem& sys_id,
                                    const fles::SubsystemFormatFLES& sys_ver);

  void check_against_last_values(const fles::HeaderFormatIdentifier& hdr_id,
                                 const fles::HeaderFormatVersion& hdr_ver,
                                 const uint16_t& eq_id,
                                 const fles::MicrosliceFlags& flags,
                                 const fles::Subsystem& sys_id,
                                 const fles::SubsystemFormatFLES& sys_ver);
};

#endif // MSAVALIDATOR_HPP
