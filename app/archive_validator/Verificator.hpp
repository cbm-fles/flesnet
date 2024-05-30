/**
 * @file Verificator.hpp
 */

#pragma once

#include "MicrosliceInputArchive.hpp"
#include "StorableMicroslice.hpp"
#include "TimesliceInputArchive.hpp"
#include <string>
using namespace std;

/// Contains the methods used for input/ouput verification
class Verificator {
private:
    /**
     * @brief Intended to be used with std::sort as the comparision function
     */
    bool sort_timeslice_archives(string const& lhs, string const& rhs);
    
    /**
     * @brief Detects the component the given microslice belongs to in the given timeslice
     * @returns -1 on fail, else component index
     */
    int64_t get_component_idx_of_microslice(shared_ptr<fles::StorableTimeslice> ts, shared_ptr<fles::StorableMicroslice> ms);

    /**
     * @brief Consumes the ts_archive up until to the given idx - the next get call will retrieve the timeslice at idx
     */
    void skip_to_idx(uint64_t idx,fles::TimesliceInputArchive &ts_archive);
    
    /**
     * @brief Consumes the ts_archive up until to the given idx - the next get call will retrieve the timeslice at idx
     */
    void skip_to_idx(uint64_t idx,fles::MicrosliceInputArchive &ts_archive);

    uint64_t usable_threads_ = 0;
public:
    Verificator();
    ~Verificator() = default;

    /**
     * @brief Checks input msa files against output tsa files
     * @details Takes the very first microslice of a given archive from input_archive_paths as a starting point.
     * Detects the component idx of the msarchive in a timeslice, iterates through the timeslice and compares each
     * microslice against the microslices in the microslice archive. Also verifies the overlap between following timeslices.  
     * @param input_archive_paths list of microslice archive file paths
     * @param output_archive_paths list of timeslice archive file paths
     * @param timeslice_cnt the summed up amount of timeslices to be expected in all the timeslice archives
     * @param overlap overlap size of timeslices
     * @return true if valid
     */
    bool verify_forward(std::vector<std::string> input_archive_paths, std::vector<std::string> output_archive_paths, uint64_t timeslice_cnt, uint64_t overlap = 1);
  
    /**
     * @brief Checks if each timeslice in the given timeslice archive files fulfills the expections given by the other function arguments.
     * @param output_archive_paths list of timeslice archive file paths
     * @param overlap_size overlap size of timeslices
     * @param timeslice_cnt the summed up amount of timeslices to be expected in all the timeslice archives
     * @param timeslice_size expected timeslice sizes
     * @param timeslice_components expected amount of components in all of the timeslices in the given timeslice archives
     * @return true if valid
     */
    [[nodiscard]] bool verify_ts_metadata(vector<string> output_archive_paths, uint64_t timeslice_cnt, uint64_t timeslice_size, uint64_t overlap_size, uint64_t timeslice_components) const;
};