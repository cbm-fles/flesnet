#pragma once


#include "MicrosliceInputArchive.hpp"
#include "StorableMicroslice.hpp"
#include "TimesliceInputArchive.hpp"
class Verificator {


public:
    Verificator() = default;
    ~Verificator() = default;
    bool verfiy(std::vector<std::string> input_archive_paths, std::vector<std::string> output_archive_paths, uint64_t timeslice_size, uint64_t timeslice_cnt);  
};