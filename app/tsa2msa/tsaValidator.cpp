// C++ Standard Library header files:
#include <iostream>

// System dependent header files:
#include <sysexits.h>

// Flesnet Library header files:
#include "lib/fles_ipc/TimesliceAutoSource.hpp"

// tsa2msa Library header files:
#include "tsaValidator.hpp"

tsaValidator::tsaValidator()
    : time_monotonically_increasing(true), time_strict_monotonicity(true),
      index_monotonically_increasing(true), index_strict_monotonicity(true),
      num_core_microslices_constant(true), num_components_constant(true),

      nStrictMonotonicityViolationsTime(0),
      nStrictMonotonicityViolationsIndex(0), nMonotonicityViolationsTime(0),
      nMonotonicityViolationsIndex(0), nNumCoreMicroslicesViolations(0),
      nNumComponentsViolations(0),

      last_timeslice_start_time(0), last_timeslice_index(0),
      last_timeslice_num_core_microslices(0), last_timeslice_num_components(0),

      nTimeslices(0) {}

int tsaValidator::validate(const std::unique_ptr<fles::Timeslice>& timeslice) {
  if (!timeslice) {
    std::cerr << "Error: No timeslice received." << std::endl;
    return EX_SOFTWARE;
  }

  uint64_t timesliceIndex = timeslice->index();
  uint64_t numCoreMicroslices = timeslice->num_core_microslices();
  uint64_t numComponents = timeslice->num_components();
  uint64_t start_time = timeslice->start_time();
  if (nTimeslices != 0) {
    if (start_time < last_timeslice_start_time) {
      time_monotonically_increasing = false;
      time_strict_monotonicity = false;
      nMonotonicityViolationsTime++;
      nStrictMonotonicityViolationsTime++;
    } else if (start_time == last_timeslice_start_time) {
      time_strict_monotonicity = false;
      nStrictMonotonicityViolationsTime++;
    }
  }
  last_timeslice_start_time = start_time;

  if (nTimeslices != 0) {
    if (timesliceIndex < last_timeslice_index) {
      index_monotonically_increasing = false;
      index_strict_monotonicity = false;
      nMonotonicityViolationsIndex++;
      nStrictMonotonicityViolationsIndex++;
    } else if (timesliceIndex == last_timeslice_index) {
      index_strict_monotonicity = false;
      nStrictMonotonicityViolationsIndex++;
    }
  }
  last_timeslice_index = timesliceIndex;

  if (nTimeslices != 0 &&
      numCoreMicroslices != last_timeslice_num_core_microslices) {
    num_core_microslices_constant = false;
    nNumCoreMicroslicesViolations++;
  }
  last_timeslice_num_core_microslices = numCoreMicroslices;
  if (nTimeslices != 0 && numComponents != last_timeslice_num_components) {
    num_components_constant = false;
    nNumComponentsViolations++;
  }
  last_timeslice_num_components = numComponents;
  nTimeslices++;

  return EX_OK;
}

void tsaValidator::printVerboseIntermediateResult() {
  std::cout << "Timeslice number: " << nTimeslices << std::endl;
  std::cout << "Timeslice index: " << last_timeslice_index << std::endl;
  std::cout << "Number of core microslices: "
            << last_timeslice_num_core_microslices << std::endl;
  std::cout << "Number of components: " << last_timeslice_num_components
            << std::endl;
  std::cout << "Start time: " << last_timeslice_start_time << std::endl;
  if (!time_monotonically_increasing) {
    std::cout << "Warning: Timeslice start time is not monotonically "
                 "increasing."
              << std::endl;
    std::cout << "Number of monotonicity violations: "
              << nMonotonicityViolationsTime << std::endl;
  } else if (!time_strict_monotonicity) {
    std::cout << "Error: Timeslice start time is not strictly "
                 "monotonically increasing."
              << std::endl;
    std::cout << "Number of strict monotonicity violations: "
              << nStrictMonotonicityViolationsTime << std::endl;
  }
  if (!index_monotonically_increasing) {
    std::cout << "Warning: Timeslice index is not monotonically increasing."
              << std::endl;
    std::cout << "Number of monotonicity violations: "
              << nMonotonicityViolationsIndex << std::endl;
  } else if (!index_strict_monotonicity) {
    std::cout << "Error: Timeslice index is not strictly monotonically "
                 "increasing."
              << std::endl;
    std::cout << "Number of strict monotonicity violations: "
              << nStrictMonotonicityViolationsIndex << std::endl;
  }
  if (!num_core_microslices_constant) {
    std::cout << "Warning: Number of core microslices is not constant."
              << std::endl;
    std::cout << "Number of violations: " << nNumCoreMicroslicesViolations
              << std::endl;
  }
  if (!num_components_constant) {
    std::cout << "Warning: Number of components is not constant." << std::endl;
    std::cout << "Number of violations: " << nNumComponentsViolations
              << std::endl;
  }
}

void tsaValidator::printSummary() {

  std::cout << "Number of timeslices: " << nTimeslices << std::endl;
  if (!time_monotonically_increasing) {
    std::cerr
        << "Warning: Timeslice start time is not monotonically increasing."
        << std::endl;
  } else if (!time_strict_monotonicity) {
    std::cerr << "Error: Timeslice start time is not strictly monotonically "
                 "increasing."
              << std::endl;
  }
  if (!index_monotonically_increasing) {
    std::cerr << "Warning: Timeslice index is not monotonically increasing."
              << std::endl;
  } else if (!index_strict_monotonicity) {
    std::cerr << "Error: Timeslice index is not strictly monotonically "
                 "increasing."
              << std::endl;
  }
}
