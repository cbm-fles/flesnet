/**
 * \file Parameters.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "Parameters.hpp"
#include "Timeslice.hpp"
#include "global.hpp"

#include <boost/program_options.hpp>
#include <fstream>

namespace po = boost::program_options;

/// Overloaded output operator for STL vectors.
template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

std::string const Parameters::desc() const
{
    std::stringstream st;

    st << "input nodes (" << _input_nodes.size() << "): " << _input_nodes
       << std::endl;
    st << "compute nodes (" << _compute_nodes.size() << "): " << _compute_nodes
       << std::endl;
    for (auto input_index : _input_indexes) {
        st << "this is input node " << input_index << " (of "
           << _input_nodes.size() << ")" << std::endl;
    }
    for (auto compute_index : _compute_indexes) {
        st << "this is compute node " << compute_index << " (of "
           << _compute_nodes.size() << ")" << std::endl;
    }

    return st.str();
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

    float in_data_buffer_size = 1 << _in_data_buffer_size_exp;
    float suggest_in_desc_buffer_size = in_data_buffer_size
                                        / _typical_content_size
                                        * in_desc_buffer_oversize_factor;
    uint32_t suggest_in_desc_buffer_size_exp
        = ceilf(log2f(suggest_in_desc_buffer_size));

    float relative_size = in_data_buffer_size / sizeof(MicrosliceDescriptor);
    uint32_t max_in_desc_buffer_size_exp
        = floorf(log2f(relative_size * max_desc_data_ratio));
    uint32_t min_in_desc_buffer_size_exp
        = ceilf(log2f(relative_size * min_desc_data_ratio));

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

    float cn_data_buffer_size = 1 << _cn_data_buffer_size_exp;
    float suggest_cn_desc_buffer_size
        = cn_data_buffer_size
          / (_typical_content_size * (_timeslice_size + _overlap_size))
          * cn_desc_buffer_oversize_factor;

    uint32_t suggest_cn_desc_buffer_size_exp
        = ceilf(log2f(suggest_cn_desc_buffer_size));

    float relative_size = cn_data_buffer_size
                          / sizeof(TimesliceComponentDescriptor);
    uint32_t min_cn_desc_buffer_size_exp
        = ceilf(log2f(relative_size * min_desc_data_ratio));
    uint32_t max_cn_desc_buffer_size_exp
        = floorf(log2f(relative_size * max_desc_data_ratio));

    if (suggest_cn_desc_buffer_size_exp < min_cn_desc_buffer_size_exp)
        suggest_cn_desc_buffer_size_exp = min_cn_desc_buffer_size_exp;
    if (suggest_cn_desc_buffer_size_exp > max_cn_desc_buffer_size_exp)
        suggest_cn_desc_buffer_size_exp = max_cn_desc_buffer_size_exp;

    return suggest_cn_desc_buffer_size_exp;
}

void Parameters::parse_options(int argc, char* argv[])
{
    unsigned log_level = 3;

    po::options_description generic("Generic options");
    generic.add_options()("version,V", "print version string")(
        "help,h", "produce help message")(
        "log-level,l", po::value<unsigned>(&log_level),
        "set the log level (default:3, all:0)");

    po::options_description config("Configuration");
    config.add_options()("input-index,i",
                         po::value<std::vector<unsigned> >()->multitoken(),
                         "this application's index in the list of input nodes")(
        "compute-index,c",
        po::value<std::vector<unsigned> >()->multitoken(),
        "this application's index in the list of compute nodes")(
        "input-nodes,I",
        po::value<std::vector<std::string> >()->multitoken(),
        "add host to the list of input nodes")(
        "compute-nodes,C",
        po::value<std::vector<std::string> >()->multitoken(),
        "add host to the list of compute nodes")(
        "timeslice-size", po::value<uint32_t>(&_timeslice_size),
        "global timeslice size in number of MCs")(
        "overlap-size", po::value<uint32_t>(&_overlap_size),
        "size of the overlap region in number of MCs")(
        "in-data-buffer-size-exp",
        po::value<uint32_t>(&_in_data_buffer_size_exp),
        "exp. size of the input node's data buffer in bytes")(
        "in-desc-buffer-size-exp",
        po::value<uint32_t>(&_in_desc_buffer_size_exp),
        "exp. size of the input node's descriptor buffer"
        " (number of entries)")(
        "cn-data-buffer-size-exp",
        po::value<uint32_t>(&_cn_data_buffer_size_exp),
        "exp. size of the compute node's data buffer in bytes")(
        "cn-desc-buffer-size-exp",
        po::value<uint32_t>(&_cn_desc_buffer_size_exp),
        "exp. size of the compute node's descriptor buffer"
        " (number of entries)")("typical-content-size",
                                po::value<uint32_t>(&_typical_content_size),
                                "typical number of content bytes per MC")(
        "randomize-sizes", po::value<bool>(&_randomize_sizes),
        "randomize sizes flag")("check-pattern",
                                po::value<bool>(&_check_pattern),
                                "check pattern flag")(
        "use-flib", po::value<bool>(&_use_flib), "use flib flag")(
        "max-timeslice-number", po::value<uint32_t>(&_max_timeslice_number),
        "global maximum timeslice number")(
        "processor-executable",
        po::value<std::string>(&_processor_executable),
        "name of the executable acting as timeslice processor")(
        "processor-instances", po::value<uint32_t>(&_processor_instances),
        "number of instances of the timeslice processor executable");

    po::options_description cmdline_options("Allowed options");
    cmdline_options.add(generic).add(config);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    std::ifstream ifs("flesnet.cfg");
    po::store(po::parse_config_file(ifs, config), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << cmdline_options << "\n";
        exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
        std::cout << "flesnet, version 0.0"
                  << "\n";
        exit(EXIT_SUCCESS);
    }

    if (!vm.count("input-nodes"))
        throw ParametersException("list of input nodes is empty");

    if (!vm.count("compute-nodes"))
        throw ParametersException("list of compute nodes is empty");

    _input_nodes = vm["input-nodes"].as<std::vector<std::string> >();
    _compute_nodes = vm["compute-nodes"].as<std::vector<std::string> >();

    if (vm.count("input-index"))
        _input_indexes = vm["input-index"].as<std::vector<unsigned> >();
    if (vm.count("compute-index"))
        _compute_indexes = vm["compute-index"].as<std::vector<unsigned> >();

    if (_input_nodes.empty() && _compute_nodes.empty()) {
        throw ParametersException("no node type specified");
    }

    for (auto input_index : _input_indexes) {
        if (input_index >= _input_nodes.size()) {
            std::ostringstream oss;
            oss << "input node index (" << input_index << ") out of range (0.."
                << _input_nodes.size() - 1 << ")";
            throw ParametersException(oss.str());
        }
    }

    for (auto compute_index : _compute_indexes) {
        if (compute_index >= _compute_nodes.size()) {
            std::ostringstream oss;
            oss << "compute node index (" << compute_index
                << ") out of range (0.." << _compute_nodes.size() - 1 << ")";
            throw ParametersException(oss.str());
        }
    }

    if (_in_desc_buffer_size_exp == 0)
        _in_desc_buffer_size_exp = suggest_in_desc_buffer_size_exp();

    if (_cn_desc_buffer_size_exp == 0)
        _cn_desc_buffer_size_exp = suggest_cn_desc_buffer_size_exp();

    out.setVerbosity(static_cast<einhard::LogLevel>(log_level));

    out.debug() << "input nodes (" << _input_nodes.size()
                << "): " << _input_nodes;
    out.debug() << "compute nodes (" << _compute_nodes.size()
                << "): " << _compute_nodes;
    for (auto input_index : _input_indexes) {
        out.info() << "this is input node " << input_index << " (of "
                   << _input_nodes.size() << ")";
    }
    for (auto compute_index : _compute_indexes) {
        out.info() << "this is compute node " << compute_index << " (of "
                   << _compute_nodes.size() << ")";
    }

    for (auto input_index : _input_indexes) {
        if (input_index == 0) {
            out.info() << "microslice size: " << _typical_content_size
                       << " bytes";
            out.info() << "timeslice size: (" << _timeslice_size << " + "
                       << _overlap_size << ") microslices";
            out.info() << "number of timeslices: " << _max_timeslice_number;
            out.info() << "input node buffer size: ("
                       << (1 << _in_data_buffer_size_exp) << " + "
                       << (1 << _in_desc_buffer_size_exp)
                          * sizeof(MicrosliceDescriptor) << ") bytes";
            out.info() << "compute node buffer size: ("
                       << (1 << _cn_data_buffer_size_exp) << " + "
                       << (1 << _cn_desc_buffer_size_exp)
                          * sizeof(TimesliceComponentDescriptor) << ") bytes";
        }
    }
}
