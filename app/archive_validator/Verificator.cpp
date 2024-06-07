
#include "Verificator.hpp"
#include "InputArchive.hpp"
#include "MergingSource.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceView.hpp"
#include "StorableMicroslice.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"
#include "TimesliceAutoSource.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceSource.hpp"
#include "log.hpp"
#include <algorithm>
#include <atomic>
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <future>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <string>
#include <thread>

using namespace std;

Verificator::Verificator() {
    const uint64_t max_available_threads = std::thread::hardware_concurrency();
    usable_threads_ = (max_available_threads >= 3) ? max_available_threads - 2 : 1; // keep at least 2 threads unused to prevent blocking the whole system
    usable_threads_ = usable_threads_ > 16 ? 16 : usable_threads_;
}

int64_t Verificator::get_component_idx_of_microslice(shared_ptr<fles::Timeslice> ts, shared_ptr<fles::StorableMicroslice> ms) {
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

bool Verificator::verify_ts_metadata(vector<string> output_archive_paths, uint64_t *timeslice_cnt, uint64_t timeslice_size, uint64_t overlap_size, uint64_t timeslice_components) const {
    boost::interprocess::interprocess_semaphore sem(usable_threads_);
    vector<future<bool>> thread_handles;
    atomic_uint64_t ts_cnt = 0;
    atomic_uint64_t archive_cnt = output_archive_paths.size();

    for (string& output_archive_path : output_archive_paths) {
        sem.wait();
        future<bool> handle = std::async(std::launch::async, [&archive_cnt, &ts_cnt, output_archive_path, timeslice_size, timeslice_components, overlap_size, &sem] {
            L_(info) << "Verifying metadata of: '" << output_archive_path << "' ...";
            uint64_t local_ts_idx = 0;
            fles::TimesliceInputArchive ts_archive(output_archive_path);
            std::shared_ptr<fles::StorableTimeslice> current_ts = nullptr; // timeslice to search in
            stringstream err_sstr; // for error printing

            try {
                while ((current_ts = ts_archive.get()) != nullptr) {
                    uint64_t num_core_microslice = current_ts->num_core_microslices();
                    if (num_core_microslice != timeslice_size) {
                        err_sstr << "Difference in num of core microslices:" << endl 
                            << "Expected: " << timeslice_size << endl
                            << "Found: " << num_core_microslice << endl
                            << "Timeslice archive: " << output_archive_path << endl
                            << "Timeslice IDX: " << local_ts_idx << endl;
                        throw runtime_error(err_sstr.str());
                    }

                    uint64_t num_components = current_ts->num_components();
                    if (num_components != timeslice_components) {
                        err_sstr << "Difference in num of components: " << endl
                            << "Expected: " << timeslice_components << endl
                            << "Found: " << num_components << endl
                            << "Timeslice archive: " << output_archive_path << endl
                            << "Timeslice IDX: " << local_ts_idx;
                        throw runtime_error(err_sstr.str());
                    }

                    for (uint64_t c = 0; c < current_ts->num_components(); c++) {
                        uint64_t num_microslices = current_ts->num_microslices(c);
                        uint64_t ts_overlap = num_microslices - num_core_microslice;
                        if (ts_overlap != overlap_size) {
                            err_sstr << "Difference in overlap size:" << endl
                                << "Expected: " << overlap_size << endl
                                << "Found: " << ts_overlap << endl
                                << "Timeslice archive: " << output_archive_path << endl
                                << "Timeslice IDX: " << local_ts_idx << endl;
                            throw runtime_error(err_sstr.str());
                        }
                    }
                    local_ts_idx++;
                    ts_cnt++;
                }
            } catch (runtime_error err) {
                L_(fatal) << "FAILED to verify metadata of: '" << output_archive_path << "'" << endl << err.what();
                sem.post();
                return false;
            }
            L_(info) << "Successfully verified metadata of: '" << output_archive_path << "'. " << archive_cnt.fetch_sub(1, std::memory_order_relaxed) - 1 << " remaining ...";
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

    if (*timeslice_cnt == 0) {
        *timeslice_cnt = ts_cnt;
    } else if (ts_cnt != *timeslice_cnt) {
        L_(fatal) << "Difference in num of timeslices: ";
        L_(fatal) << "Expected: " << *timeslice_cnt;
        L_(fatal) << "Found: " << ts_cnt;
        return false;
    }
    
    return true;
}

bool Verificator::verify_forward(vector<string> input_archive_paths, vector<string> output_archive_paths, uint64_t timeslice_cnt, uint64_t overlap) {
    atomic_uint64_t overall_ms_cnt_stat = 0;  // counts how many ms are validated
    atomic_uint64_t overlap_cnt_stat = 0; // counts how many overlaps between ts components were checked
    atomic_uint64_t overall_ts_cnt_stat = 0; // counts how many overlaps between ts components were checked
    boost::interprocess::interprocess_semaphore sem(usable_threads_);
    vector<future<bool>> thread_handles;

    for (string& input_archive_path : input_archive_paths) {
        sem.wait();
        future<bool> handle = std::async(std::launch::async, [this, &overall_ms_cnt_stat, &overall_ts_cnt_stat, &overlap_cnt_stat, input_archive_path, &output_archive_paths, &overlap, &timeslice_cnt, &sem] {
            fles::MicrosliceInputArchive ms_archive(input_archive_path);
            shared_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
            int64_t compontent_idx = -1;
            uint64_t ms_idx = 0;
            uint64_t ts_cnt_stat = 0; // counts how many timeslices were checked
            stringstream err_sstr; // for error printing

            vector<unique_ptr<fles::TimesliceSource>> ts_sources = {};
            for (auto p : output_archive_paths) {
                std::unique_ptr<fles::TimesliceSource> source =
                    std::make_unique<fles::TimesliceInputArchive>(p);
                ts_sources.emplace_back(std::move(source));
            }

            fles::MergingSource<fles::TimesliceSource> ts_source(std::move(ts_sources));
            shared_ptr<fles::Timeslice> current_ts = ts_source.get(); // timeslice to search in
            try {
                do {
                    for (; ms_idx < current_ts->num_core_microslices(); ms_idx++) {
                        ms = ms_archive.get();
                        if (!ms) {
                            // stringstream err_sstr;
                            err_sstr << "Failed to get next microslice" << endl
                                << "Timeslice idx: " << ts_cnt_stat << endl
                                << "Microslice archive: " << input_archive_path << endl
                                << "Microslice idx: " << ms_idx << endl;
                            throw runtime_error(err_sstr.str());
                        }
                        
                        // Here we figure out which component is associated to this microslice archive file - we have to do this only once per archive file
                        if (compontent_idx < 0) {
                            compontent_idx = get_component_idx_of_microslice(current_ts, ms);
                            if (compontent_idx < 0) {
                                err_sstr << "Could not associate timeslice component to microslice:" << endl
                                    << "Timeslice idx: " << ts_cnt_stat << endl
                                    << "Microslice archive: " << input_archive_path << endl
                                    << "Microslice idx: " << ms_idx << endl;
                                throw runtime_error(err_sstr.str());
                            }
                        }

                        fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx);
                        if (ms_in_curr_component != *ms) {
                            err_sstr << "Found inequality in core area of timeslice" << endl
                                << "Timeslice idx: " << ts_cnt_stat << endl
                                << "Microslice archive: " << input_archive_path << endl
                                << "Microslice idx: " << ms_idx << endl;
                            throw runtime_error(err_sstr.str());
                        }
                        ++overall_ms_cnt_stat;
                    }

                    shared_ptr<fles::Timeslice> next_ts = ts_source.get();

                    if (!next_ts) { // if we still have no follow up ts, we are in the very last ts - check the remaining ms in the current ts
                        for (ms_idx = 0; ms_idx < overlap; ms_idx++) {
                            ms = ms_archive.get();
                            if (!ms) {
                                err_sstr << "Could not get next microslice from MS archive" << endl
                                    << "Timeslice idx: " << ts_cnt_stat << endl
                                    << "Microslice archive: " << input_archive_path << endl
                                    << "Microslice idx: " << ms_idx << endl;
                                throw runtime_error(err_sstr.str());
                            }

                            fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                            if (*ms != ms_in_curr_component) {
                                err_sstr << "Found inequality in overlap of last timeslice" << endl
                                    << "Timeslice idx: " << ts_cnt_stat << endl
                                    << "Microslice archive: " << input_archive_path << endl
                                    << "Microslice idx: " << ms_idx << endl;
                                throw runtime_error(err_sstr.str());
                            }
                            ++overall_ms_cnt_stat;
                        }
                    } else { // else check the overlap-many *last* MS from the current_ts and overlap-many *first* MS from the next_ts - they have to be identical
                        for (ms_idx = 0; ms_idx < overlap; ms_idx++) {
                            ms = ms_archive.get();
                            if (!ms) {
                                err_sstr << "Could not get next microslice from MS archive while overlap checking" << endl
                                    << "Microslice archive: " << input_archive_path << endl
                                    << "From timeslice idx: " << ts_cnt_stat << endl  
                                    << "To timeslice idx: " << (ts_cnt_stat + 1) << endl;
                                throw runtime_error(err_sstr.str());
                            }

                            fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                            fles::MicrosliceView ms_in_next_component = next_ts->get_microslice(compontent_idx, ms_idx);
                            if (*ms != ms_in_curr_component || *ms != ms_in_next_component) {
                                err_sstr << "Overlap check failed:" << endl
                                        << "Microslice archive: " << input_archive_path << endl
                                        << "From timeslice idx: " << ts_cnt_stat << endl
                                        << "To timeslice idx: " << (ts_cnt_stat + 1) << endl;
                                throw runtime_error(err_sstr.str());
                            }
                            ++overall_ms_cnt_stat;
                        }
                        ++overlap_cnt_stat;
                    }

                    current_ts = next_ts;
                    ++ts_cnt_stat;
                    ++overall_ts_cnt_stat;
                } while (ts_cnt_stat < timeslice_cnt);
            } catch (runtime_error err) {
                L_(fatal) << err.what();
                sem.post();
                return false;
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

    L_(info) << "Verified: ";
    L_(info) << "overall microslices: " << overall_ms_cnt_stat;
    L_(info) << "overlaps count: " << overlap_cnt_stat;
    L_(info) << "overall timeslices: " << overall_ts_cnt_stat;

    return true;
}