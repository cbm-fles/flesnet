// Copyright 2012-2016 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "ChildProcessManager.hpp"
#include "FlesnetPatternGenerator.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include "shm_channel_client.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>
#include <random>
#include <string>

Application::Application(Parameters const& par,
                         volatile sig_atomic_t* signal_status)
    : par_(par), signal_status_(signal_status) {
  zmq_context_ = std::unique_ptr<void, std::function<int(void*)>>(
      zmq_ctx_new(), zmq_ctx_destroy);
  create_input_channel_senders();
  create_timeslice_buffers();
  set_node();
}

Application::~Application() = default;

void Application::create_timeslice_buffers() {
  unsigned input_size = static_cast<unsigned>(par_.inputs().size());
  unsigned output_size = static_cast<unsigned>(par_.outputs().size());

  std::vector<std::string> input_server_addresses;
  for (unsigned i = 0; i < input_size; ++i) {
    if (par_.local_only()) {
      input_server_addresses.push_back("inproc://input" + std::to_string(i));
    } else {
      input_server_addresses.push_back("tcp://" + par_.inputs().at(i).host +
                                       ":" +
                                       std::to_string(par_.base_port() + i));
    }
  }

  for (unsigned i : par_.output_indexes()) {
    auto shm_identifier = par_.outputs().at(i).path.at(0);
    auto param = par_.outputs().at(i).param;

    uint32_t datasize = 27; // 128 MiB
    if (param.count("datasize") != 0u) {
      datasize = stou(param.at("datasize"));
    }
    uint32_t descsize = 19; // 16 MiB
    if (param.count("descsize") != 0u) {
      descsize = stou(param.at("descsize"));
    }

    L_(info) << "timeslice buffer " << i
             << " size: " << human_readable_count(UINT64_C(1) << datasize)
             << " + "
             << human_readable_count(
                    (UINT64_C(1) << descsize) *
                    sizeof(fles::TimesliceComponentDescriptor));

    std::unique_ptr<TimesliceBuffer> tsb(
        new TimesliceBuffer(shm_identifier, datasize, descsize, input_size));

    start_processes(shm_identifier);
    ChildProcessManager::get().allow_stop_processes(this);

    if (par_.transport() == Transport::ZeroMQ) {
      std::unique_ptr<TimesliceBuilderZeromq> builder(
          new TimesliceBuilderZeromq(i, *tsb, input_server_addresses,
                                     output_size, par_.timeslice_size(),
                                     par_.max_timeslice_number(),
                                     signal_status_, zmq_context_.get()));
      timeslice_builders_zeromq_.push_back(std::move(builder));
    } else if (par_.transport() == Transport::LibFabric) {
#ifdef HAVE_LIBFABRIC
      std::unique_ptr<tl_libfabric::TimesliceBuilder> builder(
          new tl_libfabric::TimesliceBuilder(
              i, *tsb, par_.base_port() + i, input_size, par_.timeslice_size(),
              signal_status_, false, par_.outputs().at(i).host));
      timeslice_builders_.push_back(std::move(builder));
#else
      L_(fatal) << "flesnet built without LIBFABRIC support";
#endif
    } else {
#ifdef HAVE_RDMA
      std::unique_ptr<TimesliceBuilder> builder(new TimesliceBuilder(
          i, *tsb, par_.base_port() + i, input_size, par_.timeslice_size(),
          signal_status_, false, par_.monitor_uri()));
      timeslice_builders_.push_back(std::move(builder));
#else
      L_(fatal) << "flesnet built without RDMA support";
#endif
    }

    timeslice_buffers_.push_back(std::move(tsb));
  }
}

void Application::create_input_channel_senders() {
  std::vector<std::string> output_hosts;
  for (unsigned int i = 0; i < par_.outputs().size(); ++i) {
    output_hosts.push_back(par_.outputs().at(i).host);
  }

  std::vector<std::string> output_services;
  for (unsigned int i = 0; i < par_.outputs().size(); ++i) {
    output_services.push_back(std::to_string(par_.base_port() + i));
  }

  for (size_t c = 0; c < par_.input_indexes().size(); ++c) {
    unsigned index = par_.input_indexes().at(c);

    auto scheme = par_.inputs().at(index).scheme;
    auto param = par_.inputs().at(index).param;

    if (scheme == "shm") {
      auto shm_identifier = par_.inputs().at(index).path.at(0);
      auto channel = std::stoul(par_.inputs().at(index).path.at(1));

      if (shm_devices_.count(shm_identifier) == 0u) {
        try {
          shm_devices_.insert(std::make_pair(
              shm_identifier,
              std::make_shared<flib_shm_device_client>(shm_identifier)));
          L_(info) << "using shared memory (" << shm_identifier << ")";
        } catch (std::exception const& e) {
          L_(error) << "exception while connecting to shared memory: "
                    << e.what();
        }
      }

      data_sources_.push_back(
          std::unique_ptr<InputBufferReadInterface>(new flib_shm_channel_client(
              shm_devices_.at(shm_identifier), channel)));
    } else if (scheme == "pgen") {
      uint32_t datasize = 27; // 128 MiB
      if (param.count("datasize") != 0u) {
        datasize = stou(param.at("datasize"));
      }
      uint32_t descsize = 19; // 16 MiB
      if (param.count("descsize") != 0u) {
        descsize = stou(param.at("descsize"));
      }
      uint32_t size_mean = 1024; // 1 kiB
      if (param.count("mean") != 0u) {
        size_mean = stou(param.at("mean"));
      }
      uint32_t size_var = 0;
      if (param.count("var") != 0u) {
        size_var = stou(param.at("var"));
      }
      uint32_t pattern = 0;
      if (param.count("pattern") != 0u) {
        pattern = stou(param.at("pattern"));
      }
      uint64_t delay_ns = 0;
      if (param.count("delay") != 0u) {
        delay_ns = stoul(param.at("delay"));
      }

      L_(info) << "input buffer " << index
               << " size: " << human_readable_count(UINT64_C(1) << datasize)
               << " + "
               << human_readable_count((UINT64_C(1) << descsize) *
                                       sizeof(fles::MicrosliceDescriptor));
      L_(info) << "microslice size: " << human_readable_count(size_mean)
               << " +/- " << human_readable_count(size_var);

      data_sources_.push_back(std::unique_ptr<InputBufferReadInterface>(
          new FlesnetPatternGenerator(datasize, descsize, index, size_mean,
                                      (pattern != 0), (size_var != 0),
                                      delay_ns)));
    } else {
      L_(fatal) << "unknown input scheme: " << scheme;
    }

    uint32_t overlap_size = 1;
    if (param.count("overlap") != 0u) {
      overlap_size = stou(param.at("overlap"));
    }

    if (par_.transport() == Transport::ZeroMQ) {
      std::string listen_address =
          "tcp://*:" + std::to_string(par_.base_port() + index);
      if (par_.local_only()) {
        listen_address = "inproc://input" + std::to_string(index);
      }
      std::unique_ptr<ComponentSenderZeromq> sender(new ComponentSenderZeromq(
          index, *(data_sources_.at(c).get()), listen_address,
          par_.timeslice_size(), overlap_size, par_.max_timeslice_number(),
          signal_status_, zmq_context_.get()));
      component_senders_zeromq_.push_back(std::move(sender));
    } else if (par_.transport() == Transport::LibFabric) {
#ifdef HAVE_LIBFABRIC
      std::unique_ptr<tl_libfabric::InputChannelSender> sender(
          new tl_libfabric::InputChannelSender(
              index, *(data_sources_.at(c).get()), output_hosts,
              output_services, par_.timeslice_size(), overlap_size,
              par_.max_timeslice_number(), par_.inputs().at(c).host));
      input_channel_senders_.push_back(std::move(sender));
#else
      L_(fatal) << "flesnet built without LIBFABRIC support";
#endif
    } else {
#ifdef HAVE_RDMA
      std::unique_ptr<InputChannelSender> sender(new InputChannelSender(
          index, *(data_sources_.at(c).get()), output_hosts, output_services,
          par_.timeslice_size(), overlap_size, par_.max_timeslice_number(),
          par_.monitor_uri()));
      input_channel_senders_.push_back(std::move(sender));
#else
      L_(fatal) << "flesnet built without RDMA support";
#endif
    }
  }
}

void Application::run() {
// Do not spawn additional thread if only one is needed, simplifies
// debugging
#if defined(HAVE_RDMA) || defined(HAVE_LIBFABRIC)
  if (timeslice_builders_.size() == 1 && input_channel_senders_.empty()) {
    L_(debug) << "using existing thread for single timeslice builder";
    (*timeslice_builders_[0])();
    return;
  };
  if (input_channel_senders_.size() == 1 && timeslice_builders_.empty()) {
    L_(debug) << "using existing thread for single input channel sender";
    (*input_channel_senders_[0])();
    return;
  };
#endif

  // FIXME: temporary code, need to implement interrupt
  boost::thread_group threads;
  std::vector<boost::unique_future<void>> futures;
  bool stop = false;

#if defined(HAVE_RDMA) || defined(HAVE_LIBFABRIC)
  for (auto& buffer : timeslice_builders_) {
    boost::packaged_task<void> task(std::ref(*buffer));
    futures.push_back(task.get_future());
    threads.add_thread(new boost::thread(std::move(task)));
  }

  for (auto& buffer : input_channel_senders_) {
    boost::packaged_task<void> task(std::ref(*buffer));
    futures.push_back(task.get_future());
    threads.add_thread(new boost::thread(std::move(task)));
  }
#endif

  for (auto& buffer : timeslice_builders_zeromq_) {
    boost::packaged_task<void> task(std::ref(*buffer));
    futures.push_back(task.get_future());
    threads.add_thread(new boost::thread(std::move(task)));
  }

  for (auto& buffer : component_senders_zeromq_) {
    boost::packaged_task<void> task(std::ref(*buffer));
    futures.push_back(task.get_future());
    threads.add_thread(new boost::thread(std::move(task)));
  }

  L_(debug) << "threads started: " << threads.size();

  while (!futures.empty()) {
    auto it = boost::wait_for_any(futures.begin(), futures.end());
    try {
      it->get();
    } catch (const std::exception& e) {
      L_(fatal) << "exception from thread: " << e.what();
      stop = true;
    }
    futures.erase(it);
    if (stop) {
      threads.interrupt_all();
    }
  }

  threads.join_all();
}

void Application::start_processes(const std::string& shared_memory_identifier) {
  const std::string processor_executable = par_.processor_executable();
  assert(!processor_executable.empty());
  for (uint_fast32_t i = 0; i < par_.processor_instances(); ++i) {
    std::stringstream index;
    index << i;
    ChildProcess cp = ChildProcess();
    cp.owner = this;
    boost::split(cp.arg, processor_executable, boost::is_any_of(" \t"),
                 boost::token_compress_on);
    cp.path = cp.arg.at(0);
    for (auto& arg : cp.arg) {
      boost::replace_all(arg, "%s", shared_memory_identifier);
      boost::replace_all(arg, "%i", index.str());
    }
    ChildProcessManager::get().start_process(cp);
  }
}
