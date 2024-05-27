
#include "Verificator.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceView.hpp"
#include "StorableMicroslice.hpp"
#include "StorableTimeslice.hpp"
#include "TimesliceInputArchive.hpp"
#include "log.hpp"
#include <algorithm>
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <future>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>


bool Verificator::sort_timeslice_archives(string const& lhs, string const& rhs) {
    fles::TimesliceInputArchive lhs_archive(lhs);
    std::unique_ptr<fles::StorableTimeslice> lhs_first_ts  = lhs_archive.get();
    fles::MicrosliceView lhs_first_ms = lhs_first_ts->get_microslice(0, 0);

    fles::TimesliceInputArchive rhs_archive(rhs);
    std::unique_ptr<fles::StorableTimeslice> rhs_first_ts  = rhs_archive.get();
    fles::MicrosliceView rhs_first_ms = rhs_first_ts->get_microslice(0, 0);

    return lhs_first_ms.desc().idx < rhs_first_ms.desc().idx;
}

int64_t Verificator::get_component_idx_of_microslice(shared_ptr<fles::StorableTimeslice> ts, shared_ptr<fles::StorableMicroslice> ms) {
    for (uint64_t c = 0; c < ts->num_components(); c++) {
        for (uint64_t ms_idx = 0; ms_idx < ts->num_microslices(c); ms_idx++) {
            fles::MicrosliceView ms_in_timeslice = ts->get_microslice(c, ms_idx);
            if (ms_in_timeslice == *ms) {
                return c;
            }
        }
    }
    return -1;
}

void Verificator::skip_to_idx(uint64_t idx,fles::TimesliceInputArchive &ts_archive) {
    for (uint64_t i = 0; i < idx; i++) {
        ts_archive.get();
    }
}

void Verificator::skip_to_idx(uint64_t idx,fles::MicrosliceInputArchive &ms_archive) {
    for (uint64_t i = 0; i < idx; i++) {
        ms_archive.get();
    }
}

bool Verificator::verify_ts_metadata(vector<string> output_archive_paths, uint64_t timeslice_cnt, uint64_t timeslice_size, uint64_t overlap_size, uint64_t timeslice_components) {
    const uint64_t max_available_threads = std::thread::hardware_concurrency();
    const uint64_t usable_threads = (max_available_threads >= 3) ? max_available_threads - 2 : 1; // keep at least 2 threads unused to prevent blocking the whole system
    const uint64_t using_threads = usable_threads > 16 ? 16 : usable_threads;
    L_(info) << "System provides " << max_available_threads << " concurrent threads. Will use: " << using_threads;
    boost::interprocess::interprocess_semaphore sem(using_threads);
    vector<future<bool>> thread_handles;
    atomic_uint64_t ts_cnt = 0;
    atomic_uint64_t archive_cnt = output_archive_paths.size();
    for (string& output_archive_path : output_archive_paths) {
        sem.wait();
        future<bool> handle = std::async(std::launch::async, [&archive_cnt, &ts_cnt, output_archive_path, timeslice_size, timeslice_components, overlap_size, &sem] {
            L_(info) << "Verify metadata of: " << output_archive_path << " ...";
            uint64_t local_ts_idx = 0;
            fles::TimesliceInputArchive ts_archive(output_archive_path);
            std::shared_ptr<fles::StorableTimeslice> current_ts = nullptr; // timeslice to search in
            while ((current_ts = ts_archive.get()) != nullptr) {
                uint64_t num_core_microslice = current_ts->num_core_microslices();
                if (num_core_microslice != timeslice_size) {
                    L_(fatal) << "Difference in num of core microslices: ";
                    L_(fatal) << "Expeced: " << timeslice_size;
                    L_(fatal) << "Found: " << num_core_microslice;
                    L_(fatal) << "Timeslice archive: " << output_archive_path;
                    L_(fatal) << "Timeslice IDX: " << local_ts_idx;
                    sem.post();
                    return false;
                }

                uint64_t num_components = current_ts->num_components();
                if (num_components != timeslice_components) {
                    L_(fatal) << "Difference in num of components: ";
                    L_(fatal) << "Expeced: " << timeslice_components;
                    L_(fatal) << "Found: " << num_components;
                    L_(fatal) << "Timeslice archive: " << output_archive_path;
                    L_(fatal) << "Timeslice IDX: " << local_ts_idx;
                    sem.post();
                    return false;
                }

                for (uint64_t c = 0; c < current_ts->num_components(); c++) {
                    uint64_t num_microslices = current_ts->num_microslices(c);
                    uint64_t ts_overlap = num_microslices - num_core_microslice;
                    if (ts_overlap != overlap_size) {
                        L_(fatal) << "Difference in overlap size: ";
                        L_(fatal) << "Expeced: " << overlap_size;
                        L_(fatal) << "Found: " << ts_overlap;
                        L_(fatal) << "Timeslice archive: " << output_archive_path;
                        L_(fatal) << "Timeslice IDX: " << local_ts_idx;
                        sem.post();
                        return false;
                    }
                }
                local_ts_idx++;
                ts_cnt++;
                // L_(info) << ts_cnt << endl;
            }
            L_(info) << archive_cnt.fetch_sub(1, std::memory_order_relaxed) - 1 << " left ..." << endl;
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
    if (ts_cnt != timeslice_cnt) {
        L_(fatal) << "Difference in num of timeslices: ";
        L_(fatal) << "Expeced: " << timeslice_cnt;
        L_(fatal) << "Found: " << ts_cnt;
        return false;
    }

    return true;

}

bool Verificator::verify_forward(vector<string> input_archive_paths, vector<string> output_archive_paths, uint64_t timeslice_cnt, uint64_t overlap) {
    std::sort(output_archive_paths.begin(), output_archive_paths.end(),  
        [this](string lhs, string rhs) { return sort_timeslice_archives(lhs, rhs); });

    /* ToDo: Currently depricated boilerplate for parallel processing - reimplement parallel checking */

    // vector<unique_ptr<fles::MicrosliceInputArchive>> input_archives;
    // vector<unique_ptr<fles::TimesliceInputArchive>> output_archives;
    // uint64_t entry_node_cnt = input_archive_paths.size();
    // uint64_t process_node_cnt = output_archive_paths.size();
    // if (entry_node_cnt == 0) {
    //     L_(fatal) << "No output archives provided.";
    //     throw std::runtime_error("No input archives provided.");
    // }

    // if (process_node_cnt == 0) {
    //     L_(fatal) << "No output archives provided.";
    //     throw std::runtime_error("No output archives provided.");
    // }

    // vector<future<bool>> thread_handles;
    // const uint64_t sent_microslices_cnt = timeslice_size * (timeslice_cnt + overlap);
    // const uint64_t max_available_threads = std::thread::hardware_concurrency();
    // const uint64_t usable_threads = (max_available_threads >= 3) ? max_available_threads - 2 : 1; // keep at least 2 threads unused to prevent blocking the whole system
    // L_(info) << "System provides " << max_available_threads << " concurrent threads. Will use: " << usable_threads;
    // boost::interprocess::interprocess_semaphore sem(usable_threads);
    
    /* END ToDo*/


    // Variables to keep track of certain statistics
    uint64_t overall_ms_cnt_stat = 0;  // counts how many ms are validated
    uint64_t overlap_cnt_stat = 0; // counts how many overlaps between ts components were checked

    for (string& input_archive_path : input_archive_paths) {
        fles::MicrosliceInputArchive ms_archive(input_archive_path); // open microslice archive
        shared_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
        int64_t compontent_idx = -1;
        uint64_t ms_idx = 0;
        uint64_t ts_file_select = 0;
        uint64_t ts_offset = 0; // counts how many timeslices were checked
        uint64_t ts_cnt_stat = 0; // counts how many timeslices were checked


        string output_archive_path = output_archive_paths[ts_file_select];
        fles::TimesliceInputArchive ts_archive(output_archive_path);
        skip_to_idx(ts_offset, ts_archive);
        std::shared_ptr<fles::StorableTimeslice> current_ts = ts_archive.get(); // timeslice to search in

        do {
            for (; ms_idx < current_ts->num_core_microslices(); ms_idx++) {
                ms = ms_archive.get();
                if (!ms) {
                    L_(fatal) << "Failed to get next microslice";
                    // ToDo: print stats
                    return false;
                }
                
                if (compontent_idx < 0) { // here we figure out which component is associated to this microslice archive file - we have to do this only once
                    compontent_idx = get_component_idx_of_microslice(current_ts, ms);
                    if (compontent_idx < 0) {
                        L_(fatal) << "Could not associate timeslice component to microslice: ";
                        L_(fatal) << "Timeslice archive: " << output_archive_path;
                        L_(fatal) << "Timeslice idx: " << ts_cnt_stat;
                        L_(fatal) << "Microslice archive: " << input_archive_path;
                        L_(fatal) << "Microslice idx: " << ms_idx;
                        return false;
                    }
                }

                fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx);

                if (ms_in_curr_component != *ms) {
                    L_(info) << "Found inequality in core area of timeslice";
                    L_(fatal) << "Timeslice archive: " << output_archive_path;
                    L_(fatal) << "Timeslice idx: " << ts_cnt_stat;
                    L_(fatal) << "Microslice archive: " << input_archive_path;
                    L_(fatal) << "Microslice idx: " << ms_idx;
                    return false;
                }
                overall_ms_cnt_stat++;
            }

            // overlap check
            uint64_t next_ts_archive_select = (ts_file_select + 1) % output_archive_paths.size();
            if (next_ts_archive_select == 0) {
                ts_offset++;
            }
            fles::TimesliceInputArchive ts_next_archive(output_archive_paths[next_ts_archive_select]);
            skip_to_idx(ts_offset, ts_next_archive);
            std::shared_ptr<fles::StorableTimeslice> next_ts = ts_next_archive.get();

            if (!next_ts) { // if we still have no follow up ts, we are in the very last ts - check the remaining ms in the current ts
                for (ms_idx = 0; ms_idx < overlap; ms_idx++) {
                    ms = ms_archive.get();
                    if (!ms) {
                        L_(fatal) << "Could not get next microslice from MS archive";
                        // ToDo: Print stats
                        return false;
                    }

                    fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                    if (*ms != ms_in_curr_component) {
                        L_(fatal) << "Found inequality in overlap of last timeslice";
                        // ToDo: Print stats
                        return false;
                    }
                    overall_ms_cnt_stat++;
                }
            } else { // else check the overlap-many *last* MS from the current_ts and overlap-many *first* MS from the next_ts - they have to be identical
                for (ms_idx = 0; ms_idx < overlap; ms_idx++) {
                    ms = ms_archive.get();
                    if (!ms) {
                        L_(fatal) << "Could not get next microslice from MS archive while overlap checking";
                        // ToDo: Print stats
                        return false;
                    }

                    fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                    fles::MicrosliceView ms_in_next_component = next_ts->get_microslice(compontent_idx, ms_idx);
                    if (*ms != ms_in_curr_component || *ms != ms_in_next_component) {
                        L_(fatal) << "Overlap check failed: ";
                        L_(fatal) << "Timeslice archive: " << output_archive_path;
                        L_(fatal) << "Timeslice idx: " << ts_cnt_stat;
                        return false;
                    }
                    overall_ms_cnt_stat++;
                }
                overlap_cnt_stat++;
            }

            current_ts = next_ts;
            ts_file_select = next_ts_archive_select;
            ts_cnt_stat++;
            L_(info) << "overall_ms_cnt_stat: " << overall_ms_cnt_stat;
            L_(info) << "overlap_cnt_stat: " << overlap_cnt_stat;
        } while (ts_cnt_stat < timeslice_cnt);
    }

    L_(info) << "overall_ms_cnt_stat: " << overall_ms_cnt_stat;
    L_(info) << "overlap_cnt_stat: " << overlap_cnt_stat;
    return true;
}