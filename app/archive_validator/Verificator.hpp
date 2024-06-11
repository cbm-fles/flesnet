/**
 * @file Verificator.hpp
 */

#pragma once

#include "MicrosliceInputArchive.hpp"
#include "StorableMicroslice.hpp"
#include "TimesliceInputArchive.hpp"
#include <cstdint>
#include <string>

/// Contains the methods used for input/output verification
class Verificator {
private:
    /**
     * @brief Detects the component the given microslice belongs to in the given timeslice
     * @returns -1 on fail, else component index
     */
    int64_t get_component_idx_of_microslice(std::shared_ptr<fles::Timeslice> ts, std::shared_ptr<fles::StorableMicroslice> ms);

    /**
     * @brief Formats the given seconds into Hh:MMm:SSs 
     * @param seconds seconds to turn into a formatted string
     * @returns formatted string
     */
    std::string format_time_seconds(uint64_t seconds);

    uint64_t usable_threads_ = 0;
public:
    Verificator(uint64_t max_threads = 1);
    ~Verificator() = default;

    /**
     * @brief Checks input msa files against output tsa files
     * @details Takes the very first microslice of a given archive from input_archive_paths as a starting point.
     * Detects the component idx of the msarchive in a timeslice, iterates through the timeslice and compares each
     * microslice against the microslices in the microslice archive. Also verifies the overlap between following timeslices.  
     * @param input_archive_paths list of microslice archive file paths
     * @param output_archive_paths list of timeslice archive file paths
     * @param timeslice_cnt the summed up number of timeslices to be expected in all the timeslice archives
     * @param overlap overlap size of timeslices
     * @return true if valid
     */
    bool verify_forward(std::vector<std::string> input_archive_paths, std::vector<std::string> output_archive_paths, uint64_t timeslice_cnt, uint64_t overlap = 1);
  
    /**
     * @brief Checks if each timeslice in the given timeslice archive files fulfills the expections given by the other function arguments.
     * @param output_archive_paths list of timeslice archive file paths
     * @param overlap_size overlap size of timeslices
     * @param timeslice_cnt the summed up number of timeslices to be expected in all the timeslice archives
     * @param timeslice_size expected timeslice sizes
     * @param timeslice_components expected number of components in all of the timeslices in the given timeslice archives
     * @return true if valid 
     */
    [[nodiscard]] bool verify_ts_metadata(std::vector<std::string> output_archive_paths, uint64_t *timeslice_cnt, uint64_t timeslice_size, uint64_t overlap_size, uint64_t timeslice_components) const;
};