// #pragma once

// #include "ItemProducer.hpp"
// #include "Sink.hpp"
// #include "StorableTimeslice.hpp"
// #include "Timeslice.hpp"
// #include <boost/archive/binary_oarchive.hpp>
// #include <cstdint>
// #include <df/Connectors/ConnectorInterface.hpp>
// #include "TimesliceBuffer.hpp"
// #include "TimesliceComponentDescriptor.hpp"
// #include "TimesliceDescriptor.hpp"
// #include "TimesliceOutputArchive.hpp"
// #include "TimesliceReceiver.hpp"
// #include "TimesliceShmWorkItem.hpp"
// #include <boost/archive/text_oarchive.hpp>
// #include <cstring>
// #include <future>
// #include <memory>
// #include <mutex>
// #include <thread>
// #include <iostream>
// #include "System.hpp"
// #include "TimesliceView.hpp"
// #include "Utility.hpp"
// #include <memory>

// #include <TimesliceSource.hpp>
// #include <type_traits>
// using namespace std;
// using namespace std::placeholders;


// class MyTimeslice : public fles::StorableTimeslice {
// public:
//     // friend class fles::StorableTimeslice;

//     void set_data(std::vector<uint8_t*> data_ptr) {
//         fles::Timeslice::data_ptr_ = data_ptr;
//     }

//     void set_desc(std::vector<fles::TimesliceComponentDescriptor*> desc_ptr) {
//         fles::Timeslice::desc_ptr_ = desc_ptr;
//     }

//     void set_timeslice_descriptor(fles::TimesliceDescriptor ts_desc) {
//         fles::Timeslice::timeslice_descriptor_ = ts_desc;
//     };
// };
// using MyTimesliceArchive = fles::OutputArchive<fles::Timeslice, fles::StorableTimeslice, fles::ArchiveType::TimesliceArchive>;

// class ConnectorFromFlesnet : public ConnectorInterface {
// private:
//     std::string listen_address_;
//     // std::unique_ptr<fles::TimesliceSource> source_ = nullptr;
//     char* base_mem_addres_ = nullptr;

//     std::unique_ptr<fles::Receiver<fles::Timeslice,fles::TimesliceView>> source_ = nullptr;
//     std::future<int> ts_reading_thread_;
//     std::future<int> ts_writing_thread_;
//     std::atomic_bool stop_ = false;
//     std::atomic_bool start_reading_ = false;
//     std::mutex unmanaged_recv_mtx_;
//     std::vector<std::function<void (std::string address, std::shared_ptr<char> data, uint64_t size)>> unmanaged_recv_callbacks_;
//     std::vector<std::unique_ptr<fles::Timeslice>> timeslices;
//     zmq::context_t zmq_context_{1};
//     std::unique_ptr<fles::TimesliceSink> sink = nullptr;
//     std::unique_ptr<fles::Timeslice> last_timeslice_ = nullptr;
//     std::shared_ptr<TimesliceBuffer> ts_buffer_ = nullptr;
//     std::shared_ptr<ItemProducer> item_producer_ = nullptr;
//     // std::unique_ptr<fles::TimesliceOutputArchive> ts_sink_ = nullptr;
//     std::unique_ptr<MyTimesliceArchive> ts_sink_ = nullptr;
//     // void buffer_map_changed(BufferMap::ListElement::IO io) {
//     //     last_timeslice_.reset();
//     // };

// public:
//     void create_timeslice() {
//         if (ts_sink_ == nullptr) {
//             return;
//         }
//         cout << "create_timeslice 1" << endl;
//         vector<uint8_t*> data_ptr;
//         vector<fles::TimesliceComponentDescriptor*> desc_ptr;
//         uint64_t combined_size = 0;
//         cout << "create_timeslice 2" << endl;

//         buffer_map_->lock();
//         cout << "create_timeslice 3" << endl;

//         const auto *el = buffer_map_->get_oldest_linked_list_element();
//         cout << "create_timeslice 4" << endl;
//         cout << "el->compontent_id: " << el->compontent_id << endl;
//         const auto elements = buffer_map_->get_elements_of_component(el->compontent_id, combined_size);
//         for (auto& el : elements) {
//             cout << "address: " << el->address << endl;
//         }
//         cout << "create_timeslice 5: " << elements.size() << endl;
//         cout << "create_timeslice. component_elements.size(): " << elements.size() << endl;
//         cout << "create_timeslice. combined_size: " << combined_size << endl;

//         for (auto desc_it = elements.begin(); desc_it != elements.end(); ++desc_it) {
//             auto *const descriptor_el = *desc_it;
//             // buffer_map_->print_list_element(*desc_it);
//             if (1 == (descriptor_el->tag >> (sizeof(uint16_t) * 8))) { // referencing a descriptor
//                 // ts_shm_item->desc.push_back(descriptor_el->address);
//                 desc_ptr.push_back(reinterpret_cast<fles::TimesliceComponentDescriptor*>(global_buffer_.get() + descriptor_el->address));

//                 const auto idx = static_cast<uint16_t>(descriptor_el->tag);
//                 for (const auto& element : elements) {
//                     if (static_cast<uint16_t>(element->tag) == idx && 2 == (element->tag >> (sizeof(uint16_t) * 8))) { // is refere
//                         // ts_shm_item->data.push_back(element->address);
//                         data_ptr.push_back(reinterpret_cast<uint8_t*>(global_buffer_.get() + element->address));
//                         break;
//                     }
//                 }
//             } // else referencing data
//         }
//         cout << "create timeslice 6" << endl;
//         auto ts = make_shared<MyTimeslice>();
//         ts->set_timeslice_descriptor({
//             desc_ptr[0]->ts_num,
//             0, 100,
//             static_cast<uint32_t>(desc_ptr.size())
//         });
//         cout << "create timeslice 7" << endl;
//         cout << "desc_ptr.size(): " << desc_ptr.size() << endl;
//         cout << "data_ptr.size(): " << data_ptr.size() << endl;
//         ts->set_desc(desc_ptr);
//         ts->set_data(data_ptr);
//         cout << "create timeslice 8" << endl;

//         ts_sink_->put(reinterpret_pointer_cast<fles::StorableTimeslice>(ts));

//         cout << "ts_index: " << ts->index() << endl;
//         cout << "before remove elements" << endl;

//         buffer_map_->remove_elements(elements);
//         // buffer_map_->print_all();
//         buffer_map_->unlock();
//         cout << "create timeslice done" << endl;
//         ts_sink_ = nullptr;
//         // fles::TimesliceComponentDescriptor tsc;

//         return;
//     };

//     int set_global_buffer_map(std::shared_ptr<BufferMap> buffer_map) override {
//         if (!buffer_map) {
//             return -1;
//         }

//         buffer_map_ = buffer_map;
//         return 0;
//     }

//     void clear_last_timeslice() {
//         last_timeslice_ = nullptr;
//     };

//     void start_readimg() {
//         start_reading_ = true;
//     }

//     void start_timeslice_reading() {
//         ts_reading_thread_ = std::async([this] {
//             constexpr int sleep_timeout = 200;
//             cout << "start_timeslice_reading 1" << endl;
//             std::unique_ptr<fles::Timeslice> timeslice = source_->get();
//             cout << "start_timeslice_reading 2" << endl;

//             // global_buffer_size_ = source_->managed_shm_->get_size();
//             // cout << "start_timeslice_reading 3" << endl;

//             // global_buffer_ = std::shared_ptr<char>(reinterpret_cast<char*>(source_->managed_shm_->get_address()));
//             // cout << "start_timeslice_reading 4" << endl;

//             base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
//             cout << "start_timeslice_reading 5" << endl;

//             while(!buffer_map_) {
//                 std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
//             }
//             cout << "start_timeslice_reading 6" << endl;

//             // buffer_map_->add_elements_removed_callback([&] (const std::vector<uint64_t>& /*addresses*/) {
//             //     last_timeslice_ = nullptr;
//             // });

//             auto *metadata = buffer_map_->get_list_metadata();
//             metadata->buffer_size = global_buffer_size_;
//             timeslice.reset();
//             buffer_map_->add_elements_removed_callback([this] (const std::vector<uint64_t> &) {
//                 cout << "add_elements_removed_callback" << endl;
//                 // last_timeslice_.reset();
//                 clear_last_timeslice();
//                 // last_timeslice_ = nullptr;
//                 cout << "add_elements_removed_callback done" << endl;
//             });            // cout << "start_timeslice_reading 1" << endl;

//             // // while (!source_ && !stop_) {
//             // //     std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
//             // // }

//             // std::unique_ptr<fles::Timeslice> timeslice = source_->get();
//             // global_buffer_size_ = source_->managed_shm_->get_size();
//             // global_buffer_ = std::shared_ptr<char>(reinterpret_cast<char*>(source_->managed_shm_->get_address()));
//             // base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
//             // while(!buffer_map_) {
//             //     std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
//             // }

//             // // buffer_map_->add_elements_removed_callback([&] (const std::vector<uint64_t>& /*addresses*/) {
//             // //     last_timeslice_ = nullptr;
//             // // });

//             // auto *metadata = buffer_map_->get_list_metadata();
//             // metadata->buffer_size = global_buffer_size_;
//             // timeslice.reset();
//             // buffer_map_->add_elements_removed_callback([this] (const std::vector<uint64_t> &) {
//             //     cout << "add_elements_removed_callback" << endl;
//             //     // last_timeslice_.reset();
//             //     clear_last_timeslice();
//             //     // last_timeslice_ = nullptr;
//             //     cout << "add_elements_removed_callback done" << endl;
//             // });
//             cout << "waiting for start_reading_ ..." << endl;
//             while (!start_reading_) {
//                 std::this_thread::sleep_for(std::chrono::milliseconds(sleep_timeout));
//             }
//             // uint64_t count = 10;
//             cout << "start_reading_" << endl;

//             while (!stop_)  {
//                 // std::ostringstream sstream;
//                 // boost::archive::text_oarchive archive(sstream);

//                 // std::this_thread::sleep_for(std::chrono::milliseconds(10));
//                 // fles::TimesliceShmWorkItem item;
//                 timeslice = source_->get();
//                 // count--;
//                 if (!timeslice) {
//                     cout << "could not get timeslice" << endl;
//                     break;
//                 }
//                 cout << "got new timeslice" << endl;

//                 if (base_mem_addres_ != reinterpret_cast<char*>(source_->managed_shm_->get_address())) {
//                     base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());
//                     cerr << "!!! ConnectorFromFlesnet: Base memory address changed" << endl;
//                 }
//                 // item.shm_uuid = source_->managed_shm_uuid();
//                 // item.shm_identifier = shm_identifier_;

//                 // item.ts_desc.index = timeslice->index();
//                 // item.ts_desc.num_components = timeslice->num_components();
//                 // item.ts_desc.num_core_microslices = timeslice->num_core_microslices();


//                 // item.ts_desc.ts_pos = timeslice->;
//                 // const auto num_components = item.ts_desc.num_components;
//                 // const auto ts_pos = item.ts_desc.ts_pos;
//                 // item.data.resize(num_components);
//                 // item.desc.resize(num_components);
//                 // for (uint32_t c = 0; c < num_components; ++c) {
//                     // fles::TimesliceComponentDescriptor* tsc_desc = &get_desc(c, ts_pos);
//                     // uint8_t* tsc_data = &get_data(c, tsc_desc->offset);
//                     // item.data[c] = managed_shm_->get_handle_from_address(tsc_data);
//                     // item.desc[c] = managed_shm_->get_handle_from_address(tsc_desc);
//                 // }

//                 // std::ostringstream ostream;
//                 // {
//                 //     boost::archive::binary_oarchive oarchive(ostream);
//                 //     oarchive << item;
//                 // }
//                 // outstanding_.insert(ts_pos);

//                 const auto num_components = timeslice->num_components();
//                 auto addresses = std::shared_ptr<uint64_t>(new uint64_t[num_components * 2], std::default_delete<uint64_t[]>());
//                 auto sizes = std::shared_ptr<uint64_t>(new uint64_t[num_components * 2], std::default_delete<uint64_t[]>());
//                 auto tags = std::shared_ptr<uint32_t>(new uint32_t[num_components * 2], std::default_delete<uint32_t[]>());

//                 // tag layout:
//                 // [<is_descriptor or data> (uint16_t)] [data and descriptor have the same int here (uint16_t)]
//                 // in more detail:
//                 // ['1' is descriptor, '2' is data] [idx (set by loop variable)]
//                 // e.g.:
//                 // tag: [1][8] is a descriptor that belongs to the corrosponding data element with tag [2][8]
//                 for (uint64_t i = 0; i < num_components; i++) {
//                     auto *component_desc_ptr = timeslice->desc_ptr_.at(i);
//                     auto *component_data_ptr = timeslice->data_ptr_[i];
//                     sizes.get()[i] = sizeof(fles::TimesliceComponentDescriptor);
//                     sizes.get()[num_components + i] = timeslice->size_component(i);
//                     tags.get()[i] = static_cast<uint32_t>(1) << (sizeof(uint16_t) * 8) | static_cast<uint16_t>(i);
//                     // cout << "tag: " << tags.get()[i] <<endl;
//                     tags.get()[num_components + i] = static_cast<uint32_t>(2) << (sizeof(uint16_t) * 8) | static_cast<uint16_t>(i);

//                     addresses.get()[i] = reinterpret_cast<char*>(component_desc_ptr) - base_mem_addres_;
//                     addresses.get()[num_components + i] = reinterpret_cast<char*>(component_data_ptr) - base_mem_addres_;
//                 }
//                 buffer_map_->lock();
//                 const auto *const buffer_map_ret = buffer_map_->insert(
//                     num_components * 2,
//                     sizes.get(),
//                     addresses.get(),
//                     tags.get(),
//                     BufferMap::ListElement::IO::RX
//                 );

//                 if (buffer_map_ret == nullptr) {
//                     cout << "buffer map full" << endl;
//                 }
//                 uint64_t combined_size = 0;
//                 auto component_elements = buffer_map_->get_elements_of_component(buffer_map_ret->compontent_id, combined_size);

//                 cout << "combined size: " << combined_size << endl;
//                 cout << "component_elements.size(): " << component_elements.size() << endl;

//                 buffer_map_->unlock();

//                 last_timeslice_ = std::move(timeslice);
//                 cout << "ts index: " << last_timeslice_->index() << endl;
//                 buffer_map_update_callbacks_.call_async("", 0);
//                 // call_unmanaged_recv_cbs_sync(listen_address_, nullptr, 0);
//             }
//             cout << "stopped reading thread" << endl;
//             return int(!stop_);
//         });
//     }

//     void start_timeslice_writing() {
//         ts_writing_thread_ = std::async([this] {
//             return 0;
//         });
//     };


//     // ConnectorFromFlesnet() {
//     // }

//     ~ConnectorFromFlesnet() {
//         cout << "ConnectorFromFlesnet destructor" << endl;

//         stop_ = true;
//         // future<> objects throws an error when we wait on it, without starting it
//                     // source_->;

//         try {
//             ts_reading_thread_.wait();
//             // ts_writing_thread_.wait();
//         } catch(...){};

//         if (ts_sink_ != nullptr) {
//             cout << "ts sink released" << endl;
//                         ts_sink_->end_stream();

//             ts_sink_.release();
//             ts_sink_.reset();
//         }
//         cout << "ConnectorFromFlesnet destructor done" << endl;
//     }

//     // do nothing - the global buffer and its size is set when connected to the flesnet shared memory
//     // int set_global_buffer(std::shared_ptr<char> /*buffer*/, uint64_t /*buffer_size*/) override {
//     //     return 0;
//     // }

//     /**
//      * @brief Read from timeslice buffer
//      * @param address
//      * @return 0 on success, != 0 on failure
//      */
//     int listen_for_clients(std::string address, std::shared_ptr<char> custom_data = nullptr, uint64_t custom_data_size = 0) override {;
//         SUPRESS_UNUSED(custom_data);
//         SUPRESS_UNUSED(custom_data_size);
//         std::cout << "listen_for_clients 1" << endl;
//         listen_address_ = address;
//         WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll, 0,
//             "AutoSource at PID " +
//                 std::to_string(fles::system::current_pid())};
//         source_ = std::make_unique<fles::Receiver<fles::Timeslice,fles::TimesliceView>>(listen_address_, param);
//         std::cout << "listen_for_clients 2" << endl;
//         std::unique_ptr<fles::Timeslice> timeslice = source_->get();
//         global_buffer_size_ = source_->managed_shm_->get_size();
//         cout << "global_buffer_size_: " << global_buffer_size_ << endl;
//         global_buffer_ = std::shared_ptr<char>(reinterpret_cast<char*>(source_->managed_shm_->get_address()));
//         base_mem_addres_ = reinterpret_cast<char*>(source_->managed_shm_->get_address());

//         std::cout << "listen_for_clients 3" << endl;
//         start_timeslice_reading();
//         return 0;
//     }

//     /**
//      * @brief Get the address this connector is listening on.
//      * @details The returned address does not have to be the same as the listen address
//      * given in to the listen_for_clients() method
//      * @note the returned address will appear as the argument of connect_to_server() on a different node
//      * @param address
//      * @return 0 on success, != 0 on failure
//      */
//     std::string get_listen_address() override {
//         return listen_address_;
//     };

//     /**
//      * @brief Prepare to write to timeslice sink
//      * @note the address argument was produced by the get_listen_address() of another node
//      * @param address the address to connect to
//      * @return 0 on success, != 0 on failure
//      */
//      int connect_to_server(std::string address, std::shared_ptr<char> custom_data = nullptr, uint64_t custom_data_size = 0) override {
//         SUPRESS_UNUSED(custom_data);
//         SUPRESS_UNUSED(custom_data_size);
//         // UriComponents uri{address};
//         // uint32_t num_components = 1;
//         // uint32_t datasize = 27; // 128 MiB
//         // uint32_t descsize = 19; // 16 MiB
//         // for (auto& [key, value] : uri.query_components) {
//         //     if (key == "datasize") {
//         //         datasize = std::stoul(value);
//         //     } else if (key == "descsize") {
//         //         descsize = std::stoul(value);
//         //     } else if (key == "n") {
//         //         num_components = std::stoul(value);
//         //     } else {
//         //         throw std::runtime_error(
//         //             "query parameter not implemented for scheme " + uri.scheme +
//         //             ": " + key);
//         //     }
//         // }
//         // const auto shm_identifier = split(uri.path, "/").at(0);
//         // // sink = std::unique_ptr<fles::TimesliceSink>(
//         // //     new ManagedTimesliceBuffer(zmq_context_, shm_identifier, datasize,
//         // //                                 descsize, num_components));
//         // const auto producer_address = "inproc://" + shm_identifier;
//         // ts_buffer_ = make_shared<TimesliceBuffer>(zmq_context_,
//         //                                 producer_address,
//         //                                 shm_identifier,
//         //                                 datasize,
//         //                                 descsize,
//         //                                 num_components);
//         // buffer_map_->get_list_metadata()->buffer_size = ts_buffer_->managed_shm_->get_size();
//         // item_producer_ = make_shared<ItemProducer>(zmq_context_, producer_address);

//         fles::ArchiveCompression compression = fles::ArchiveCompression::None;
//         ts_sink_ = std::make_unique<MyTimesliceArchive>(
//             address, compression);
//         return 0;
//     }

//     int recv(std::string /*address*/, std::shared_ptr<char> /*buff*/, uint64_t /*size*/) override {
//         throw std::runtime_error("not implemented");
//         return -1;
//     }

//     int send(std::string /*address*/, std::shared_ptr<char> /*buff*/, uint64_t /*size*/) override {
//         throw std::runtime_error("not implemented");
//         // // auto *list_element = buffer_map_->get_oldest_linked_list_element();
//         // fles::TimesliceShmWorkItem item;
//         // fles::TimesliceDescriptor ts_desc;

//         // //! @todo fill out ts_desc based on received data
//         // //! @todo fill out item

//         // item.ts_desc = ts_desc;
//         // std::ostringstream ostream;
//         // {
//         //     boost::archive::binary_oarchive oarchive(ostream);
//         //     oarchive << item;
//         // }
//         //  //! @todo figure out the correct ts_pos_ so receiving does not go wrong
//         // auto ts_pos = 0;
//         // item_producer_->send_work_item(ts_pos, ostream.str());
//         return -1;
//     }

// };