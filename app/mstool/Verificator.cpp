
#include "Verificator.hpp"
#include "MicrosliceView.hpp"
#include "StorableTimeslice.hpp"
#include "TimesliceInputArchive.hpp"
#include <cstdint>
#include <iostream>
#include <exception>
#include <memory>
#include <stdexcept>


using namespace std;
bool Verificator::verfiy(vector<string> input_archive_paths, vector<string> output_archive_paths, uint64_t timeslice_size, uint64_t timeslice_cnt) {
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

    std::unique_ptr<fles::StorableMicroslice> ms = nullptr; // microslice to search for
    std::unique_ptr<fles::StorableTimeslice> ts = nullptr; // timeslice to search in

    // Check if all the microslices from the TS archives can be found in the input MS archives; check if no additional or falsy data was created.
    try {
        for (std::string& output_archive_path : output_archive_paths) {
            fles::TimesliceInputArchive ts_archive(output_archive_path);
            uint64_t ts_cnt = 0;
            while ((ts = (ts_archive).get()) != nullptr) {
                std::cout << "Output TS archive: " << output_archive_path << " - ts_cnt: "<< ts_cnt << std::endl;
                for (uint64_t c = 0; c < ts->num_components(); c++) {
                    for (uint64_t ms_idx = 0; ms_idx < ts->num_microslices(c); ms_idx++) {
                        fles::MicrosliceView ms_in_component = ts->get_microslice(c, ms_idx);
                        for (std::string& input_archive_path : input_archive_paths) {
                            fles::MicrosliceInputArchive ms_archive(input_archive_path);
                            while ((ms = ms_archive.get()) != nullptr) {
                                if (ms_in_component == *ms) {
                                    goto found_ts_in_ms_archive;
                                    break;
                                }
                            }
                        }
                        found_ts_in_ms_archive:;
                    }
                }
                ts_cnt++;
            }
        }
    } catch (exception& e) {
        std::cout << e.what() << std::endl;
        return false;
    }

    return true;

    // ms = nullptr;
    // ts = nullptr;

    // x. Check if all input microslices from the input MS archives can be found in the TS archives
    // uint64_t sent_microslices_cnt = timeslice_size + timeslice_cnt; // NOTE: is it ok to ignore overlap?
    // try {
    //     for (std::string& input_archive_path : input_archive_paths) {
    //         fles::MicrosliceInputArchive ms_archive(input_archive_path);
    //         uint64_t cnt = 0;
    //         while ((ms = ms_archive.get()) != nullptr && cnt < sent_microslices_cnt) {
    //             for (std::string& output_archive_path : output_archive_paths) {
    //                 fles::TimesliceInputArchive ts_archive(output_archive_path);
    //                 while ((ts = (ts_archive).get()) != nullptr) {
    //                     for (uint64_t c = 0; c < 2; c++) {
    //                         for (uint64_t ms_idx = 0; ms_idx < 1; ms_idx++) {
    //                             fles::MicrosliceView ms_in_component = ts->get_microslice(c, ms_idx);
    //                             std::cout << ms->desc().idx << ":" << ms_in_component.desc().idx <<  std::endl;
    //                             if (ms_in_component == *ms) {
    //                                 std::cout << "found" << std::endl;
    //                             }
    //                         }
    //                     }
    //                 }
    //             }
    //         }
    //     }
    //     return true;
    // } catch (exception& e) {
    //     std::cout << e.what() << std::endl;
    //     return false;
    // }
   
    // return true;
}