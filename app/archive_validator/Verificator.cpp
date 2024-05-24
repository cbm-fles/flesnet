
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


// void print_timeslice_archive_info(std::string ts_archive_path) {
//     fles::TimesliceInputArchive ts_archive(ts_archive_path);
//     std::unique_ptr<fles::StorableTimeslice> ts = nullptr; // timeslice to search in
//     uint64_t timeslice_cnt = 0;
//     uint64_t components_cnt = 0;
//     uint64_t microslices_in_ts_cnt = 0;
//     while ((ts = (ts_archive).get()) != nullptr) {
//         if (timeslice_cnt == 0) {
//             components_cnt = ts->num_components();
//             microslices_in_ts_cnt = ts->num_microslices(components_cnt - 1);
//         }
//         timeslice_cnt++;
//     }

//     L_(info) << "Printing info for timeslice archive: " << ts_archive_path;
//     L_(info) << "Timeslice cnt.: " << timeslice_cnt;
//     L_(info) << "Microslices per timeslice: " << microslices_in_ts_cnt;
//     L_(info) << "Components per timeslice: " << components_cnt;
// }


// uint64_t get_num_timeslices_in_archive(std::string ts_archive_path) {
//     fles::TimesliceInputArchive ts_archive(ts_archive_path);
//     uint64_t cnt = 0;
//     while (ts_archive.get() != nullptr) {
//         cnt++;
//     }
//     return cnt;
// }

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

bool Verificator::verify_forward(vector<string> input_archive_paths, vector<string> output_archive_paths, uint64_t timeslice_size, uint64_t timeslice_cnt, uint64_t overlap) {
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
    uint64_t ms_cnt_stat = 0;  // counts how many ms are validated
    uint64_t overlap_cnt_stat = 0; // counts how many overlaps between ts components were checked
    for (string& input_archive_path : input_archive_paths) {
        fles::MicrosliceInputArchive ms_archive(input_archive_path); // open microslice archive
        shared_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
        int64_t compontent_idx = -1;
        uint64_t ms_idx = 0;
        uint64_t ts_file_select = 0;
        uint64_t ts_offset = 0; // counts how many timeslices were checked
        uint64_t ts_cnt_stat = 0; // counts how many timeslices were checked

        do {
            string output_archive_path = output_archive_paths[ts_file_select];
            L_(info) << output_archive_path;
            fles::TimesliceInputArchive ts_archive(output_archive_path);
            skip_to_idx(ts_offset, ts_archive);
            std::shared_ptr<fles::StorableTimeslice> current_ts = ts_archive.get(); // timeslice to search in

            for (; ms_idx < current_ts->num_core_microslices(); ms_idx++) {
                ms = ms_archive.get();
                if (!ms) {
                    L_(info) << "Failed to get next microslice";
                    // ToDo: print stats
                    return false;
                }
                
                if (compontent_idx < 0) { // here we figure out which component is associated to this microslice archive file - we have to do this only once
                    compontent_idx = get_component_idx_of_microslice(current_ts, ms);
                    if (compontent_idx < 0) {
                        L_(info) << "Could not associate timeslice component to microslice";
                        L_(info) << "Timeslice archive: " << output_archive_path;
                        L_(info) << "Timeslice idx: " << ts_cnt_stat;
                        L_(info) << "Microslice archive: " << input_archive_path;
                        L_(info) << "Microslice idx: " << ms_idx;
                        return false;
                    }
                }

                fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx);

                if (ms_in_curr_component != *ms) {
                    L_(info) << "Found inequality in core area of timeslice";
                    // ToDo: Print stats
                    return false;
                }
                ms_cnt_stat++;
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
                    fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                    if (*ms != ms_in_curr_component) {
                        L_(info) << "Found inequality in overlap of last timeslice";
                        // ToDo: Print stats
                        return false;
                    }
                    ms_cnt_stat++;
                }
            } else { // else check the overlap-many *last* MS from the current_ts and overlap-many *first* MS from the next_ts - they have to be identical
                for (ms_idx = 0; ms_idx < overlap; ms_idx++) {
                    ms = ms_archive.get();
                    if (!ms) {
                        L_(info) << "Could not get next microslice while overlap checking";
                        // ToDo: Print stats
                        return false;
                    }

                    fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                    fles::MicrosliceView ms_in_next_component = next_ts->get_microslice(compontent_idx, ms_idx);
                    if (*ms != ms_in_curr_component || *ms != ms_in_next_component) {
                        L_(info) << "Could not associate timeslice component to microslice";
                        L_(info) << "Timeslice archive: " << output_archive_path;
                        L_(info) << "Timeslice idx: " << ts_cnt_stat;
                        return false;
                    }
                    ms_cnt_stat++;
                }
                overlap_cnt_stat++;
            }
            current_ts = next_ts;
            ts_file_select = next_ts_archive_select;
            ts_cnt_stat++;
        } while (ts_cnt_stat < timeslice_cnt);
    }
    L_(info) << "ms_cnt_stat: " << ms_cnt_stat;
    L_(info) << "overlap_cnt_stat: " << overlap_cnt_stat;
    return true;
}