// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "MicrosliceDescriptor.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "Utility.hpp"
#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <log.hpp>

namespace po = boost::program_options;

std::string const Parameters::desc() const
{
    std::stringstream st;

    st << "input nodes (" << input_nodes_.size() << "): " << input_nodes_
       << std::endl;
    st << "compute nodes (" << compute_nodes_.size() << "): " << compute_nodes_
       << std::endl;
    for (auto input_index : input_indexes_) {
        st << "this is input node " << input_index << " (of "
           << input_nodes_.size() << ")" << std::endl;
    }
    for (auto compute_index : compute_indexes_) {
        st << "this is compute node " << compute_index << " (of "
           << compute_nodes_.size() << ")" << std::endl;
    }

    return st.str();
}

uint32_t Parameters::suggest_in_data_buffer_size_exp()
{
    constexpr float buffer_ram_usage_ratio = 0.05;

    // ensure value in sensible range
    constexpr uint32_t max_in_data_buffer_size_exp = 30; // 30: 1 GByte
    constexpr uint32_t min_in_data_buffer_size_exp = 20; // 20: 1 MByte

    float total_ram = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
    float suggest_in_data_buffer_size =
        buffer_ram_usage_ratio * total_ram / input_indexes_.size();

    uint32_t suggest_in_data_buffer_size_exp =
        ceilf(log2f(suggest_in_data_buffer_size));

    if (suggest_in_data_buffer_size_exp > max_in_data_buffer_size_exp)
        suggest_in_data_buffer_size_exp = max_in_data_buffer_size_exp;
    if (suggest_in_data_buffer_size_exp < min_in_data_buffer_size_exp)
        suggest_in_data_buffer_size_exp = min_in_data_buffer_size_exp;

    return suggest_in_data_buffer_size_exp;
}

uint32_t Parameters::suggest_cn_data_buffer_size_exp()
{
    constexpr float buffer_ram_usage_ratio = 0.05;

    // ensure value in sensible range
    constexpr uint32_t max_cn_data_buffer_size_exp = 30; // 30: 1 GByte
    constexpr uint32_t min_cn_data_buffer_size_exp = 20; // 20: 1 MByte

    float total_ram = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
    float suggest_cn_data_buffer_size =
        buffer_ram_usage_ratio * total_ram /
        (compute_indexes_.size() * input_nodes_.size());

    uint32_t suggest_cn_data_buffer_size_exp =
        ceilf(log2f(suggest_cn_data_buffer_size));

    if (suggest_cn_data_buffer_size_exp > max_cn_data_buffer_size_exp)
        suggest_cn_data_buffer_size_exp = max_cn_data_buffer_size_exp;
    if (suggest_cn_data_buffer_size_exp < min_cn_data_buffer_size_exp)
        suggest_cn_data_buffer_size_exp = min_cn_data_buffer_size_exp;

    return suggest_cn_data_buffer_size_exp;
}

uint32_t Parameters::suggest_in_desc_buffer_size_exp()
{
    // make desc buffer larger by this factor to account for data size
    // fluctuations
    constexpr float in_desc_buffer_oversize_factor = 4.0;

    // ensure value in sensible range
    constexpr float max_desc_data_ratio = 1.0;
    constexpr float min_desc_data_ratio = 0.1;

    static_assert(min_desc_data_ratio <= max_desc_data_ratio,
                  "invalid range for desc_data_ratio");

    float in_data_buffer_size = UINT64_C(1) << in_data_buffer_size_exp_;
    float suggest_in_desc_buffer_size = in_data_buffer_size /
                                        typical_content_size_ *
                                        in_desc_buffer_oversize_factor;
    uint32_t suggest_in_desc_buffer_size_exp =
        ceilf(log2f(suggest_in_desc_buffer_size));

    float relative_size =
        in_data_buffer_size / sizeof(fles::MicrosliceDescriptor);
    uint32_t max_in_desc_buffer_size_exp =
        floorf(log2f(relative_size * max_desc_data_ratio));
    uint32_t min_in_desc_buffer_size_exp =
        ceilf(log2f(relative_size * min_desc_data_ratio));

    if (suggest_in_desc_buffer_size_exp > max_in_desc_buffer_size_exp)
        suggest_in_desc_buffer_size_exp = max_in_desc_buffer_size_exp;
    if (suggest_in_desc_buffer_size_exp < min_in_desc_buffer_size_exp)
        suggest_in_desc_buffer_size_exp = min_in_desc_buffer_size_exp;

    return suggest_in_desc_buffer_size_exp;
}

uint32_t Parameters::suggest_cn_desc_buffer_size_exp()
{
    // make desc buffer larger by this factor to account for data size
    // fluctuations
    constexpr float cn_desc_buffer_oversize_factor = 8.0;

    // ensure value in sensible range
    constexpr float min_desc_data_ratio = 0.1;
    constexpr float max_desc_data_ratio = 1.0;

    static_assert(min_desc_data_ratio <= max_desc_data_ratio,
                  "invalid range for desc_data_ratio");

    float cn_data_buffer_size = UINT64_C(1) << cn_data_buffer_size_exp_;
    float suggest_cn_desc_buffer_size =
        cn_data_buffer_size /
        (typical_content_size_ * (timeslice_size_ + overlap_size_)) *
        cn_desc_buffer_oversize_factor;

    uint32_t suggest_cn_desc_buffer_size_exp =
        ceilf(log2f(suggest_cn_desc_buffer_size));

    float relative_size =
        cn_data_buffer_size / sizeof(fles::TimesliceComponentDescriptor);
    uint32_t min_cn_desc_buffer_size_exp =
        ceilf(log2f(relative_size * min_desc_data_ratio));
    uint32_t max_cn_desc_buffer_size_exp =
        floorf(log2f(relative_size * max_desc_data_ratio));

    if (suggest_cn_desc_buffer_size_exp < min_cn_desc_buffer_size_exp)
        suggest_cn_desc_buffer_size_exp = min_cn_desc_buffer_size_exp;
    if (suggest_cn_desc_buffer_size_exp > max_cn_desc_buffer_size_exp)
        suggest_cn_desc_buffer_size_exp = max_cn_desc_buffer_size_exp;

    return suggest_cn_desc_buffer_size_exp;
}

void Parameters::parse_options(int argc, char* argv[])
{
    unsigned log_level = 2;
    std::string log_file;
    std::string config_file;

    po::options_description generic("Generic options");
    auto generic_add = generic.add_options();
    generic_add("version,V", "print version string");
    generic_add("help,h", "produce help message");
    generic_add("log-level,l", po::value<unsigned>(&log_level),
                "set the log level (default:2, all:0)");
    generic_add("log-file,L", po::value<std::string>(&log_file),
                "name of target log file");
    generic_add(
        "config-file,f",
        po::value<std::string>(&config_file)->default_value("flesnet.cfg"),
        "name of a configuration file.");

    po::options_description config("Configuration");
    auto config_add = config.add_options();
    config_add("input-index,i",
               po::value<std::vector<unsigned>>()->multitoken(),
               "this application's index in the list of input nodes");
    config_add("compute-index,c",
               po::value<std::vector<unsigned>>()->multitoken(),
               "this application's index in the list of compute nodes");
    config_add("input-nodes,I",
               po::value<std::vector<std::string>>()->multitoken(),
               "add host to the list of input nodes");
    config_add("compute-nodes,C",
               po::value<std::vector<std::string>>()->multitoken(),
               "add host to the list of compute nodes");
    config_add("timeslice-size", po::value<uint32_t>(&timeslice_size_),
               "global timeslice size in number of microslices");
    config_add("overlap-size", po::value<uint32_t>(&overlap_size_),
               "size of the overlap region in number of microslices");
    config_add("in-data-buffer-size-exp",
               po::value<uint32_t>(&in_data_buffer_size_exp_),
               "exp. size of the input node's data buffer in bytes");
    config_add("in-desc-buffer-size-exp",
               po::value<uint32_t>(&in_desc_buffer_size_exp_),
               "exp. size of the input node's descriptor buffer"
               " (number of entries)");
    config_add("cn-data-buffer-size-exp",
               po::value<uint32_t>(&cn_data_buffer_size_exp_),
               "exp. size of the compute node's data buffer in bytes");
    config_add("cn-desc-buffer-size-exp",
               po::value<uint32_t>(&cn_desc_buffer_size_exp_),
               "exp. size of the compute node's descriptor buffer"
               " (number of entries)");
    config_add("typical-content-size",
               po::value<uint32_t>(&typical_content_size_),
               "typical number of content bytes per microslice");
    config_add("input-shm", po::value<std::string>(&input_shm_),
               "name of a shared memory to use as data source");
    config_add("standalone", po::value<bool>(&standalone_),
               "standalone mode flag");
    config_add("max-timeslice-number,n",
               po::value<uint32_t>(&max_timeslice_number_),
               "global maximum timeslice number");
    config_add("processor-executable,e",
               po::value<std::string>(&processor_executable_),
               "name of the executable acting as timeslice processor");
    config_add("processor-instances",
               po::value<uint32_t>(&processor_instances_),
               "number of instances of the timeslice processor executable");
    config_add("base-port", po::value<uint32_t>(&base_port_),
               "base IP port to use for listening");
    config_add("zeromq,z", po::value<bool>(&zeromq_), "use zeromq transport");
    config_add("generate-ts-patterns", po::value<bool>(&generate_ts_patterns_),
               "generate pattern for ts");
    config_add("random-ts-sizes", po::value<bool>(&random_ts_sizes_),
               "generate ts with random sizes");
    config_add("use-libfabric",
               po::value<bool>(&use_libfabric_)->default_value(false),
               "use libfabric transport implementation");

    po::options_description cmdline_options("Allowed options");
    cmdline_options.add(generic).add(config);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    po::notify(vm);

    std::ifstream ifs(config_file.c_str());
    if (!ifs) {
        throw ParametersException("cannot open config file: " + config_file);
    } else {
        po::store(po::parse_config_file(ifs, config), vm);
        notify(vm);
    }

    if (vm.count("help")) {
        std::cout << cmdline_options << "\n";
        exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
        std::cout << "flesnet, version 0.0"
                  << "\n";
        exit(EXIT_SUCCESS);
    }

    logging::add_console(static_cast<severity_level>(log_level));
    if (vm.count("log-file")) {
        L_(info) << "logging output to " << log_file;
        if (log_level < 3) {
            log_level = 3;
            L_(info) << "increased file log level to " << log_level;
        }
        logging::add_file(log_file, static_cast<severity_level>(log_level));
    }

    if (timeslice_size_ < 1) {
        throw ParametersException("timeslice size cannot be zero");
    }

#ifndef RDMA
    if (!zeromq_ && !use_libfabric_) {
        throw ParametersException("flesnet built without RDMA support");
    }
#endif
#ifndef LIBFABRIC
    if (!zeromq_ && use_libfabric_) {
        throw ParametersException("flesnet built without LIBFABRIC support");
    }
#endif

    if (standalone_) {
        input_nodes_ = std::vector<std::string>{"127.0.0.1"};
        input_indexes_ = std::vector<unsigned>{0};
        compute_nodes_ = std::vector<std::string>{"127.0.0.1"};
        compute_indexes_ = std::vector<unsigned>{0};
        if (zeromq_) {
            throw ParametersException(
                "no zeromq transport in stand-alone mode");
        }
    } else {
        if (!vm.count("input-nodes"))
            throw ParametersException("list of input nodes is empty");

        if (!vm.count("compute-nodes"))
            throw ParametersException("list of compute nodes is empty");

        input_nodes_ = vm["input-nodes"].as<std::vector<std::string>>();
        compute_nodes_ = vm["compute-nodes"].as<std::vector<std::string>>();

        if (vm.count("input-index"))
            input_indexes_ = vm["input-index"].as<std::vector<unsigned>>();
        if (vm.count("compute-index"))
            compute_indexes_ = vm["compute-index"].as<std::vector<unsigned>>();

        if (input_nodes_.empty() && compute_nodes_.empty()) {
            throw ParametersException("no node type specified");
        }

        for (auto input_index : input_indexes_) {
            if (input_index >= input_nodes_.size()) {
                std::ostringstream oss;
                oss << "input node index (" << input_index
                    << ") out of range (0.." << input_nodes_.size() - 1 << ")";
                throw ParametersException(oss.str());
            }
        }

        for (auto compute_index : compute_indexes_) {
            if (compute_index >= compute_nodes_.size()) {
                std::ostringstream oss;
                oss << "compute node index (" << compute_index
                    << ") out of range (0.." << compute_nodes_.size() - 1
                    << ")";
                throw ParametersException(oss.str());
            }
        }
    }

    if (!compute_nodes_.empty() && processor_executable_.empty())
        throw ParametersException("processor executable not specified");

    if (in_data_buffer_size_exp_ == 0 && input_shm().empty()) {
        in_data_buffer_size_exp_ = suggest_in_data_buffer_size_exp();
    }
    if (in_data_buffer_size_exp_ != 0 && !input_shm().empty()) {
        L_(warning) << "using shared memory buffers, in_data_buffer_size_exp "
                       "will be ignored";
        in_data_buffer_size_exp_ = 0;
    }

    if (cn_data_buffer_size_exp_ == 0)
        cn_data_buffer_size_exp_ = suggest_cn_data_buffer_size_exp();

    if (in_desc_buffer_size_exp_ == 0 && input_shm().empty()) {
        in_desc_buffer_size_exp_ = suggest_in_desc_buffer_size_exp();
    }
    if (in_desc_buffer_size_exp_ != 0 && !input_shm().empty()) {
        L_(warning) << "using shared memory buffers, in_desc_buffer_size_exp "
                       "will be ignored";
        in_desc_buffer_size_exp_ = 0;
    }

    if (cn_desc_buffer_size_exp_ == 0)
        cn_desc_buffer_size_exp_ = suggest_cn_desc_buffer_size_exp();

    if (!standalone_) {
        L_(debug) << "input nodes (" << input_nodes_.size()
                  << "): " << boost::algorithm::join(input_nodes_, " ");
        L_(debug) << "compute nodes (" << compute_nodes_.size()
                  << "): " << boost::algorithm::join(compute_nodes_, " ");
        for (auto input_index : input_indexes_) {
            L_(info) << "this is input node " << input_index << " (of "
                     << input_nodes_.size() << ")";
        }
        for (auto compute_index : compute_indexes_) {
            L_(info) << "this is compute node " << compute_index << " (of "
                     << compute_nodes_.size() << ")";
        }

        for (auto input_index : input_indexes_) {
            if (input_index == 0) {
                print_buffer_info();
            }
        }
    } else {
        print_buffer_info();
    }
}

void Parameters::print_buffer_info()
{
    L_(info) << "microslice size: "
             << human_readable_count(typical_content_size_);
    L_(info) << "timeslice size: (" << timeslice_size_ << " + " << overlap_size_
             << ") microslices";
    L_(info) << "number of timeslices: " << max_timeslice_number_;
    if (input_shm().empty()) {
        L_(info) << "input node buffer size: "
                 << human_readable_count(UINT64_C(1)
                                         << in_data_buffer_size_exp_)
                 << " + " << human_readable_count(
                                 (UINT64_C(1) << in_desc_buffer_size_exp_) *
                                 sizeof(fles::MicrosliceDescriptor));
    }
    L_(info) << "compute node buffer size: "
             << human_readable_count(UINT64_C(1) << cn_data_buffer_size_exp_)
             << " + " << human_readable_count(
                             (UINT64_C(1) << cn_desc_buffer_size_exp_) *
                             sizeof(fles::TimesliceComponentDescriptor));
}
