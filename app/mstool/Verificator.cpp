
#include "Verificator.hpp"
#include "MicrosliceView.hpp"
#include "StorableTimeslice.hpp"
#include "TimesliceInputArchive.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <limits>
#include <future>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

using namespace std;
bool Verificator::verify(vector<string> input_archive_paths, vector<string> output_archive_paths, uint64_t timeslice_size, uint64_t timeslice_cnt, uint64_t overlap) {
    vector<unique_ptr<fles::MicrosliceInputArchive>> input_archives;
    vector<unique_ptr<fles::TimesliceInputArchive>> output_archives;
    uint64_t entry_node_cnt = input_archive_paths.size();
    uint64_t process_node_cnt = output_archive_paths.size();

    if (entry_node_cnt == 0) {
        throw std::runtime_error("No input archives provided.");
    }

    if (process_node_cnt == 0) {
        throw std::runtime_error("No output archives provided.");
    }

    vector<future<bool>> thread_handles;
    const uint64_t sent_microslices_cnt = timeslice_size * timeslice_cnt + overlap;
    const uint64_t max_available_threads = std::thread::hardware_concurrency();
    const uint64_t usable_threads = (max_available_threads >= 3) ? max_available_threads - 2 : 1; // keep at least 2 threads unused to prevent blocking the whole system
    std::cout << "System provides " << max_available_threads << " concurrent threads. Will use: " << usable_threads << std::endl;
    
    boost::interprocess::interprocess_semaphore sem(usable_threads);
    
    // Check if all the microslices from the TS archives can be found in the input MS archives; check if no additional or falsy data was created.
    for (std::string& output_archive_path : output_archive_paths) {
        sem.wait();
        std::cout << "Checking '" << output_archive_path <<"' against inputs ..." << std::endl;
        future<bool> handle = std::async(std::launch::async, [output_archive_path, &input_archive_paths, &sent_microslices_cnt, &sem] {
            std::unique_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
            std::unique_ptr<fles::StorableTimeslice> ts = nullptr; // timeslice to search in
            fles::TimesliceInputArchive ts_archive(output_archive_path);
            while ((ts = (ts_archive).get()) != nullptr) {
                for (uint64_t c = 0; c < ts->num_components(); c++) {
                    for (uint64_t ms_idx = 0; ms_idx < ts->num_microslices(c); ms_idx++) {
                        fles::MicrosliceView ms_in_component = ts->get_microslice(c, ms_idx);
                        for (std::string input_archive_path : input_archive_paths) {
                            fles::MicrosliceInputArchive ms_archive(input_archive_path);
                            uint64_t ms_cnt = 0;
                            while ((ms = ms_archive.get()) != nullptr && ms_cnt < sent_microslices_cnt) {
                                if (ms_in_component == *ms) {
                                    goto found_ts_in_ms_archive;
                                }
                                ms_cnt++;
                            }
                        }
                        sem.post();
                        return false;
                        found_ts_in_ms_archive:;
                    }
                }
            }
            sem.post();
            return true;
        });
        thread_handles.push_back(std::move(handle));
    }

    for (auto& handle : thread_handles) {
        if (!handle.get()) {
            return false;
        }
    }

    thread_handles.clear();
    
    // Check if all input microslices from the input MS archives can be found in the TS archives
    for (std::string& input_archive_path : input_archive_paths) {
        sem.wait();
        std::cout << "Checking '" << input_archive_path <<"' against outputs ..." << std::endl;
        future<bool> handle = std::async(std::launch::async, [input_archive_path, &output_archive_paths, &sent_microslices_cnt, &sem] {
            std::unique_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
            std::unique_ptr<fles::StorableTimeslice> ts = nullptr; // timeslice to search in

            fles::MicrosliceInputArchive ms_archive(input_archive_path);
            uint64_t ms_cnt = 0;
            while ((ms = ms_archive.get()) != nullptr && ms_cnt < sent_microslices_cnt) {
                for (std::string& output_archive_path : output_archive_paths) {
                    fles::TimesliceInputArchive ts_archive(output_archive_path);
                    while ((ts = (ts_archive).get()) != nullptr) {                    
                        for (uint64_t c = 0; c < ts->num_components(); c++) {                       
                            for (uint64_t ms_idx = 0; ms_idx < ts->num_microslices(c); ms_idx++) {                       
                                fles::MicrosliceView ms_in_component = ts->get_microslice(c, ms_idx);
                                if (ms_in_component == *ms) {
                                    goto found_ms_in_ts_archive;
                                }
                            }
                        }
                    }
                }
                sem.post();
                return false;
                found_ms_in_ts_archive:;
                ms_cnt++;
            }
            sem.post();
            return true;
        });
        thread_handles.push_back(std::move(handle));
    }

    for (auto& handle : thread_handles) {
        if (!handle.get()) {
            return false;
        }
    }
    return true;
}

uint64_t Verificator::find_lowest_ms_index_in_ts_archive(string tsa_path, uint64_t eq_id) {
    fles::TimesliceInputArchive ts_archive(tsa_path);
    std::unique_ptr<fles::StorableTimeslice> ts = nullptr;
    
    uint64_t lowest_idx = UINT64_MAX;
    bool found_one_valid_ms = false;
    while ((ts = (ts_archive).get()) != nullptr) {
        for (uint64_t c = 0; c < ts->num_components(); c++) {
            for (uint64_t i = 0; i < ts->num_microslices(c); i++) {
                fles::MicrosliceView ms_in_component = ts->get_microslice(c, i);
                uint64_t ms_idx = ms_in_component.desc().idx;
                uint64_t ms_eq_id = ms_in_component.desc().eq_id;
                if (ms_eq_id == eq_id &&  ms_idx < lowest_idx) {
                    found_one_valid_ms = true;
                    lowest_idx = ms_idx;
                }
            }
        }
    }

    if (!found_one_valid_ms) {
        throw std::runtime_error("Couldn't find a microslice with eq_id " + to_string(eq_id));
    }

    return lowest_idx;
}

uint64_t Verificator::find_highest_ms_index_in_ts_archive(string tsa_path, uint64_t eq_id) {
    fles::TimesliceInputArchive ts_archive(tsa_path);
    std::unique_ptr<fles::StorableTimeslice> ts = nullptr;
    
    uint64_t highest_idx = 0;
    bool found_one_valid_ms = false;
    while ((ts = (ts_archive).get()) != nullptr) {
        for (uint64_t c = 0; c < ts->num_components(); c++) {
            for (uint64_t i = 0; i < ts->num_microslices(c); i++) {
                fles::MicrosliceView ms_in_component = ts->get_microslice(c, i);
                uint64_t ms_idx = ms_in_component.desc().idx;
                uint64_t ms_eq_id = ms_in_component.desc().eq_id;
                if (ms_eq_id == eq_id &&  ms_idx > highest_idx) {
                    found_one_valid_ms = true;
                    highest_idx = ms_idx;
                }
            }
        }
    }

    if (!found_one_valid_ms) {
        throw std::runtime_error("Couldn't find a microslice with eq_id " + to_string(eq_id));
    }

    return highest_idx;
}