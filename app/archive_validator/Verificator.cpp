
#include "Verificator.hpp"
#include "MergingSource.hpp"
#include "Microslice.hpp"
#include "MicrosliceInputArchive.hpp"
#include "MicrosliceView.hpp"
#include "StorableMicroslice.hpp"
#include "StorableTimeslice.hpp"
#include "Timeslice.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceSource.hpp"
#include "log.hpp"
#include <atomic>
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <future>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <string>
#include <thread>
#include "TimesliceDebugger.hpp"

using namespace std;

// Macro to print floats with a fixed number of digits after the decimal point and resets the output stream to default behavior again
#define STREAM_FLOAT(p, f) setprecision(p) << fixed << (f) << setprecision(-1) << defaultfloat

Verificator::Verificator(uint64_t max_threads) {
    if (max_threads != 0) {
        usable_threads_ = max_threads;   
    } else {
        usable_threads_ = std::thread::hardware_concurrency();
    }
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
    atomic_uint64_t archive_cnt_stat = output_archive_paths.size();

    for (string& output_archive_path : output_archive_paths) {
        sem.wait();
        future<bool> handle = std::async(std::launch::async, [&archive_cnt_stat, &ts_cnt, output_archive_path, timeslice_size, timeslice_components, overlap_size, &sem] {
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
            L_(info) << "Successfully verified metadata of: '" << output_archive_path << "'. " << archive_cnt_stat.fetch_sub(1, std::memory_order_relaxed) - 1 << " remaining ...";
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

string Verificator::format_time_seconds(uint64_t seconds) {
    stringstream sstream;
    uint64_t hour = seconds / 3600;
    uint16_t min = (seconds - hour * 3600) / 60;
    uint16_t sec = seconds  - hour * 3600 - min * 60;
    sstream << hour << "h:"
        << setw(2) << setfill('0') << min << "m:"
        << setw(2) << setfill('0') << sec << "s";  
    return sstream.str();
}

/**
 * @todo This method is bloated because of variables used to keep track of the current validation progress
 * and the printing of said progress or in case of a failure, printing the failure.
 * Would make sense to come up with a solution to pull that out.
 * Variables related to progress reports have "_stat" appended to their name (stat = statistics)
 */
bool Verificator::verify_forward(vector<string> input_archive_paths, vector<string> output_archive_paths, uint64_t timeslice_cnt, uint64_t overlap) {
    atomic_uint64_t overall_ms_cnt_stat = 0;  // counts how many ms are validated - summed up accrross the whole validation process
    atomic_uint64_t overlap_cnt_stat = 0; // counts how many overlaps between ts components were checked
    atomic_uint64_t overall_components_cnt_stat = 0; // counts how many timesliced were validated
    atomic_uint64_t archive_cnt_stat = 0; // counts how many archives were validated
    const uint64_t log_interval_stat = 100; // every n microslices we print a short progress report
    const uint64_t input_archives_cnt_stat = input_archive_paths.size();
    
    boost::interprocess::interprocess_semaphore sem(usable_threads_);
    vector<future<bool>> thread_handles;

    // Use MergingSource to automatically get a sorted timeslice source 
    vector<unique_ptr<fles::TimesliceSource>> ts_sources = {};
    for (auto p : output_archive_paths) {
        std::unique_ptr<fles::TimesliceSource> source =
            std::make_unique<fles::TimesliceInputArchive>(p);
        ts_sources.emplace_back(std::move(source));
    }
    fles::MergingSource<fles::TimesliceSource> ts_source(std::move(ts_sources));

    const uint64_t epoch_start_stat = chrono::duration_cast<std::chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count(); 
    for (string& input_archive_path : input_archive_paths) {
        sem.wait();
        future<bool> handle = std::async(std::launch::async, [this, &input_archives_cnt_stat, &epoch_start_stat, &archive_cnt_stat, &overall_ms_cnt_stat, &overall_components_cnt_stat, &overlap_cnt_stat, input_archive_path, &output_archive_paths, &overlap, &timeslice_cnt, &sem] {
            uint64_t local_ts_cnt_stat = 0; // counts how many timeslices were checked
            uint64_t local_ms_cnt_stat = 0; // counts the validated microslices of this specific microslice archive 
            uint64_t components_per_ts_stat = 0; // will hold the number of components per ts to calculate progress in percent
            uint64_t overall_components_to_validate_stat = 0;
            stringstream err_sstr; // for error printing

            fles::MicrosliceInputArchive ms_archive(input_archive_path);
            shared_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for

            int64_t compontent_idx = -1; // will hold the component index this microslice archive corrolates to
            uint64_t ms_idx = 0;
            vector<unique_ptr<fles::TimesliceSource>> ts_sources = {};
            for (auto p : output_archive_paths) {
                std::unique_ptr<fles::TimesliceSource> source =
                    std::make_unique<fles::TimesliceInputArchive>(p);
                ts_sources.emplace_back(std::move(source));
            }

            fles::MergingSource<fles::TimesliceSource> ts_source(std::move(ts_sources));
            shared_ptr<fles::Timeslice> current_ts = ts_source.get(); // timeslice to search in
            components_per_ts_stat = current_ts->num_components();
            overall_components_to_validate_stat = components_per_ts_stat * timeslice_cnt;
            try {
                do {
                    for (; ms_idx < current_ts->num_core_microslices(); ms_idx++) {
                        ms = ms_archive.get();
                        if (!ms) {
                            err_sstr << "Failed to get next microslice:" << endl
                                << "Microslice #" << ms_idx <<" in timeslice #" << local_ts_cnt_stat << " (TS idx: " << current_ts->index() << ")" << endl
                                << "Microslice archive: " << input_archive_path << " microslice #" << local_ms_cnt_stat;
                            throw runtime_error(err_sstr.str());
                        }
                        
                        // Here we figure out which component is associated to this microslice archive file - we have to do this only once per archive file
                        if (compontent_idx < 0) {
                            compontent_idx = get_component_idx_of_microslice(current_ts, ms);
                            if (compontent_idx < 0) {
                                err_sstr << "Could not associate timeslice component to microslice:" << endl
                                    << "Microslice #" << ms_idx <<" in timeslice #" << local_ts_cnt_stat << " (TS idx: " << current_ts->index() << ")" << endl
                                    << "Microslice archive: " << input_archive_path << " microslice #" << local_ms_cnt_stat;
                                throw runtime_error(err_sstr.str());
                            }
                            L_(info) << "MS archive \"" << input_archive_path << "\" corrolates to TS component: " << compontent_idx;
                        }

                        fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx);
                        if (ms_in_curr_component != *ms) {
                            err_sstr << "Found inequality in core area of timeslice:" << endl
                                << "Microslice #" << ms_idx <<" in timeslice #" << local_ts_cnt_stat << " (TS idx: " << current_ts->index() << ")" << endl
                                << "Microslice archive: " << input_archive_path << " microslice #" << local_ms_cnt_stat;
                            throw runtime_error(err_sstr.str());
                        }

                        ++local_ms_cnt_stat;
                        uint64_t last_overall_ms_cnt_stat = overall_ms_cnt_stat.fetch_add(1); 
                        if (((last_overall_ms_cnt_stat + 1) % log_interval_stat) == 0u) {
                            uint64_t epoch_now = chrono::duration_cast<std::chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
                            uint64_t t_diff = epoch_now - epoch_start_stat;
                            double microslices_per_sec = double(last_overall_ms_cnt_stat) / double(t_diff);
                            double progress = (double(overall_components_cnt_stat) / double(overall_components_to_validate_stat)) * 100.0;
                            double estimted_time = (100.0 / progress) * t_diff - t_diff;
                            L_(info) << "--- Status (" << STREAM_FLOAT(2, progress)  << "% - " << format_time_seconds(estimted_time) << " remaining): ---" << endl 
                                << last_overall_ms_cnt_stat + 1 << " microslices validated"  << " (" << STREAM_FLOAT(2, microslices_per_sec) << " microslices per sec)" << endl
                                << archive_cnt_stat << "/"  << input_archives_cnt_stat << " input archives validated";
                        }
                    }

                    shared_ptr<fles::Timeslice> next_ts = ts_source.get();

                    if (!next_ts) { // if we have no follow up ts, we are in the very last ts - check the remaining ms in the current ts
                        for (ms_idx = 0; ms_idx < overlap; ms_idx++) {
                            ms = ms_archive.get();
                            if (!ms) {
                                err_sstr << "Could not get next microslice from MS archive:" << endl
                                    << "Microslice #" << ms_idx <<" in timeslice #" << local_ts_cnt_stat << " (TS idx: " << current_ts->index() << ")" << endl
                                    << "Microslice archive: " << input_archive_path << " microslice #" << local_ms_cnt_stat;
                                throw runtime_error(err_sstr.str());
                            }

                            fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                            if (*ms != ms_in_curr_component) {
                                err_sstr << "Found inequality in overlap of last timeslice:" << endl
                                    << "Microslice #" << ms_idx <<" in timeslice #" << local_ts_cnt_stat << " (TS idx: " << current_ts->index() << ")" << endl
                                    << "Microslice archive: " << input_archive_path << " microslice #" << local_ms_cnt_stat;
                                throw runtime_error(err_sstr.str());
                            }

                            ++local_ms_cnt_stat;
                            uint64_t last_overall_ms_cnt_stat = overall_ms_cnt_stat.fetch_add(1); 
                            if (((last_overall_ms_cnt_stat + 1) % log_interval_stat) == 0u) {
                                uint64_t epoch_now = chrono::duration_cast<std::chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
                                uint64_t t_diff = epoch_now - epoch_start_stat;
                                double microslices_per_sec = double(last_overall_ms_cnt_stat)/ double(t_diff);
                                double progress = (double(overall_components_cnt_stat) / double(overall_components_to_validate_stat)) * 100.0;
                                double estimted_time = (100.0 / progress) * t_diff - t_diff;
                                L_(info) << "--- Status (" << STREAM_FLOAT(2, progress)  << "% - " << format_time_seconds(estimted_time) << " remaining): ---" << endl 
                                    << last_overall_ms_cnt_stat + 1 << " microslices validated"  << " (" << STREAM_FLOAT(2, microslices_per_sec) << " microslices per sec)" << endl
                                    << archive_cnt_stat << "/"  << input_archives_cnt_stat << " input archives validated";
                            }
                        }
                    } else { // else check the overlap-many *last* MS from the current_ts and overlap-many *first* MS from the next_ts - they have to be identical
                        for (ms_idx = 0; ms_idx < overlap; ms_idx++) {
                            ms = ms_archive.get();
                            if (!ms) {
                                err_sstr << "Could not get next microslice from MS archive while overlap checking:" << endl
                                    << "Microslice #" << ms_idx <<" in timeslice #" << local_ts_cnt_stat << " (TS idx: " << current_ts->index() << ")" << endl
                                    << "Microslice archive: " << input_archive_path << " microslice #" << local_ms_cnt_stat << endl;
                                throw runtime_error(err_sstr.str());
                            }

                            fles::MicrosliceView ms_in_curr_component = current_ts->get_microslice(compontent_idx, ms_idx + current_ts->num_core_microslices());
                            fles::MicrosliceView ms_in_next_component = next_ts->get_microslice(compontent_idx, ms_idx);
                            if (*ms != ms_in_curr_component || *ms != ms_in_next_component) {
                                err_sstr << "Overlap check failed:" << endl
                                        << "Microslice archive: " << input_archive_path << " microslice #" << local_ms_cnt_stat << endl
                                        << "From timeslice #: " << local_ts_cnt_stat << " (TS idx: " << current_ts->index() << ")" << endl
                                        << "To timeslice #: " << (local_ts_cnt_stat + 1) << " (TS idx: " << next_ts->index() << ")" << endl
                                        << "MS in TS # " << local_ts_cnt_stat << ":" << endl << MicrosliceDescriptorDump(ms_in_curr_component.desc()) << endl
                                        << "MS in TS #:" << (local_ts_cnt_stat + 1) << ":" << endl <<  MicrosliceDescriptorDump(ms_in_next_component.desc()) << endl
                                        << "MS in archive:" << endl << MicrosliceDescriptorDump(ms->desc());
                                if (ms->content() != ms_in_curr_component.content()) {
                                    err_sstr << R"(Content of "Archive MS" and "Current TS MS" is different)"<< endl;
                                }

                                if (ms->content() != ms_in_next_component.content()) {
                                    err_sstr << R"(Content of "Archive MS" and "Next TS MS" is different)"<< endl;
                                }

                                if (ms_in_curr_component.content() != ms_in_next_component.content()) {
                                    err_sstr << R"(Content of "Current TS MS" and "Next TS MS" is different)"<< endl;
                                }
                                throw runtime_error(err_sstr.str());
                            }
                            
                            ++local_ms_cnt_stat;
                            uint64_t last_overall_ms_cnt_stat = overall_ms_cnt_stat.fetch_add(1); 
                            if (((last_overall_ms_cnt_stat + 1) % log_interval_stat) == 0u) {
                                uint64_t epoch_now = chrono::duration_cast<std::chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
                                uint64_t t_diff = epoch_now - epoch_start_stat;
                                double microslices_per_sec = double(last_overall_ms_cnt_stat)/ double(t_diff);
                                double progress = (double(overall_components_cnt_stat) / double(overall_components_to_validate_stat)) * 100.0;
                                double estimted_time = (100.0 / progress) * t_diff - t_diff;
                                L_(info) << "--- Status (" << STREAM_FLOAT(2, progress)  << "% - " << format_time_seconds(estimted_time) << " remaining): ---" << endl 
                                    << last_overall_ms_cnt_stat + 1 << " microslices validated"  << " (" << STREAM_FLOAT(2, microslices_per_sec) << " microslices per sec)" << endl
                                    << archive_cnt_stat << "/"  << input_archives_cnt_stat << " input archives validated";
                            }
                        }
                        ++overlap_cnt_stat;
                    }

                    current_ts = next_ts;
                    ++local_ts_cnt_stat;
                    ++overall_components_cnt_stat;
                } while (local_ts_cnt_stat < timeslice_cnt);
            } catch (runtime_error err) {
                L_(fatal) << err.what();
                ++archive_cnt_stat;
                sem.post();
                return false;
            }
            ++archive_cnt_stat;
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
    uint64_t epoch_now = chrono::duration_cast<std::chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
    uint64_t elapsed = epoch_now - epoch_start_stat;
    L_(info) << "Verified:";
    L_(info) << "Time elapsed: " << format_time_seconds(elapsed);
    L_(info) << "Overall microslices: " << overall_ms_cnt_stat;
    L_(info) << "Overlaps count: " << overlap_cnt_stat;
    L_(info) << "Overall TS components: " << overall_components_cnt_stat;

    return true;
}