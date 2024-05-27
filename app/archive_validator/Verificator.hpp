#pragma once


#include "MicrosliceInputArchive.hpp"
#include "StorableMicroslice.hpp"
#include "TimesliceInputArchive.hpp"
#include <string>
using namespace std;

class Verificator {
private:
    // used with std::sort(..., ..., )
    bool sort_timeslice_archives(string const& lhs, string const& rhs);
    int64_t get_component_idx_of_microslice(shared_ptr<fles::StorableTimeslice> ts, shared_ptr<fles::StorableMicroslice> ms);
    void skip_to_idx(uint64_t idx,fles::TimesliceInputArchive &ts_archive);
    void skip_to_idx(uint64_t idx,fles::MicrosliceInputArchive &ts_archive);

public:
    Verificator() = default;
    ~Verificator() = default;
    bool verify_forward(std::vector<std::string> input_archive_paths, std::vector<std::string> output_archive_paths, uint64_t timeslice_cnt, uint64_t overlap = 1);
    bool verify_ts_metadata(vector<string> output_archive_paths, uint64_t timeslice_cnt, uint64_t timeslice_size, uint64_t overlap_size, uint64_t timeslice_components);
};