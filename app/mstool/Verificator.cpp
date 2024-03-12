
#include "Verificator.hpp"
#include "MicrosliceView.hpp"
#include "StorableTimeslice.hpp"
#include "TimesliceInputArchive.hpp"
#include "log.hpp"
#include <algorithm>
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <cstdint>
#include <stdexcept>
#include <future>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>


void print_timeslice_archive_info(std::string ts_archive_path) {
    fles::TimesliceInputArchive ts_archive(ts_archive_path);
    std::unique_ptr<fles::StorableTimeslice> ts = nullptr; // timeslice to search in
    uint64_t timeslice_cnt = 0;
    uint64_t components_cnt = 0;
    uint64_t microslices_in_ts_cnt = 0;
    while ((ts = (ts_archive).get()) != nullptr) {
        if (timeslice_cnt == 0) {
            components_cnt = ts->num_components();
            microslices_in_ts_cnt = ts->num_microslices(components_cnt - 1);
        }
        timeslice_cnt++;
    }

    L_(info) << "Printing info for timeslice archive: " << ts_archive_path;
    L_(info) << "Timeslice cnt.: " << timeslice_cnt;
    L_(info) << "Microslices per timeslice: " << microslices_in_ts_cnt;
    L_(info) << "Components per timeslice: " << components_cnt;
}


uint64_t get_num_timeslices_in_archive(std::string ts_archive_path) {
    fles::TimesliceInputArchive ts_archive(ts_archive_path);
    uint64_t cnt = 0;
    while (ts_archive.get() != nullptr) {
        cnt++;
    }
    return cnt;
}


bool Verificator::verify(vector<string> input_archive_paths, vector<string> output_archive_paths, uint64_t timeslice_size, uint64_t timeslice_cnt, uint64_t overlap) {
    vector<unique_ptr<fles::MicrosliceInputArchive>> input_archives;
    vector<unique_ptr<fles::TimesliceInputArchive>> output_archives;
    uint64_t entry_node_cnt = input_archive_paths.size();
    uint64_t process_node_cnt = output_archive_paths.size();
    if (entry_node_cnt == 0) {
        L_(fatal) << "No output archives provided.";
        throw std::runtime_error("No input archives provided.");
    }

    if (process_node_cnt == 0) {
        L_(fatal) << "No output archives provided.";
        throw std::runtime_error("No output archives provided.");
    }

    vector<future<bool>> thread_handles;
    const uint64_t sent_microslices_cnt = timeslice_size * (timeslice_cnt + overlap);
    const uint64_t max_available_threads = std::thread::hardware_concurrency();
    const uint64_t usable_threads = (max_available_threads >= 3) ? max_available_threads - 2 : 1; // keep at least 2 threads unused to prevent blocking the whole system
    L_(info) << "System provides " << max_available_threads << " concurrent threads. Will use: " << usable_threads;
    
    boost::interprocess::interprocess_semaphore sem(usable_threads);
    
    // Check if all the microslices from the TS archives can be found in the input MS archives; check if no additional or falsy data was created.
    for (std::string& output_archive_path : output_archive_paths) {
        sem.wait();
        L_(info) << "Checking '" << output_archive_path <<"' against inputs ...";
        print_timeslice_archive_info(output_archive_path);

        future<bool> handle = std::async(std::launch::async, [output_archive_path, timeslice_cnt, &input_archive_paths, &sent_microslices_cnt, &sem] {
            std::unique_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
            std::unique_ptr<fles::StorableTimeslice> ts = nullptr; // timeslice to search in
            fles::TimesliceInputArchive ts_archive(output_archive_path);
            uint64_t ts_cnt = get_num_timeslices_in_archive(output_archive_path);
            if (ts_cnt != timeslice_cnt) { // archive contains more or less timeslices as expected
                return false;
            }
            uint64_t current_timeslice = 0;
            while (current_timeslice < timeslice_cnt) {
                ts = ts_archive.get();
                if (ts == nullptr) {
                    return false;
                }
                
                for (uint64_t c = 0; c < ts->num_components(); c++) {
                    for (uint64_t ms_idx = 0; ms_idx < ts->num_microslices(c); ms_idx++) {
                        fles::MicrosliceView ms_in_component = ts->get_microslice(c, ms_idx);
                        uint64_t ms_cnt = 0;
                        for (std::string input_archive_path : input_archive_paths) {
                            fles::MicrosliceInputArchive ms_archive(input_archive_path);
                            while ((ms = ms_archive.get()) != nullptr) {
                                if (ms_in_component == *ms ||  ms_cnt >= sent_microslices_cnt) {
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
                current_timeslice++;
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
        L_(info) << "Checking '" <<  input_archive_path << "' against outputs ...";
        future<bool> handle = std::async(std::launch::async, [input_archive_path, &output_archive_paths, &timeslice_size, &timeslice_cnt, &sem] {
            std::unique_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
            std::unique_ptr<fles::StorableTimeslice> ts = nullptr; // timeslice to search in

            fles::MicrosliceInputArchive ms_archive(input_archive_path);
            uint64_t ms_cnt = 0;
            while ((ms = ms_archive.get()) != nullptr && ms_cnt < (timeslice_size * timeslice_cnt)) {
                for (std::string& output_archive_path : output_archive_paths) {
                    fles::TimesliceInputArchive ts_archive(output_archive_path);
                    while ((ts = ts_archive.get()) != nullptr) {
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