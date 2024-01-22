#pragma once


#include "MicrosliceInputArchive.hpp"
#include "StorableMicroslice.hpp"
#include "TimesliceInputArchive.hpp"
#include <string>
using namespace std;

class Verificator {
public:
    Verificator() = default;
    ~Verificator() = default;
    bool verify(std::vector<std::string> input_archive_paths, std::vector<std::string> output_archive_paths, uint64_t timeslice_size, uint64_t timeslice_cnt, uint64_t overlap = 1);
    uint64_t find_lowest_ms_index_in_ts_archive(string tsa_path, uint64_t eq_id);
    uint64_t find_highest_ms_index_in_ts_archive(string tsa_path, uint64_t eq_id);
};