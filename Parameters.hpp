#pragma once
/**
 * \file Parameters.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

namespace po = boost::program_options;

/// Overloaded output operator for STL vectors.
template<class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}


/// Run parameter exception class.
/** A ParametersException object signals an error in a given parameter
    on the command line or in a configuration file. */

class ParametersException : public std::runtime_error
{
public:
    /// The ParametersException constructor.
    explicit ParametersException(const std::string& what_arg = "")
        : std::runtime_error(what_arg) { };
};


/// Global run parameter class.
/** A Parameters object stores the information given on the command
    line or in a configuration file. */

class Parameters
{
public:
    /// The Parameters command-line parsing constructor.
    Parameters(int argc, char* argv[]) {
        parse_options(argc, argv);
    };

    Parameters(const Parameters&) = delete;
    void operator=(const Parameters&) = delete;

    /// Return a description of active nodes, suitable for debug output.
    std::string const desc() const {
        std::stringstream st;

        st << "input nodes (" << _input_nodes.size() << "): "
           << _input_nodes << std::endl;
        st << "compute nodes (" << _compute_nodes.size() << "): "
           << _compute_nodes << std::endl;
        for (auto input_index: _input_indexes) {
            st << "this is input node " << input_index << " (of "
               << _input_nodes.size() << ")" << std::endl;
        }
        for (auto compute_index: _compute_indexes) {
            st << "this is compute node " << compute_index << " (of "
               << _compute_nodes.size() << ")" << std::endl;
        }

        return st.str();
    };

    /// Retrieve the global timeslice size in number of MCs.
    uint32_t timeslice_size() const {
        return _timeslice_size;
    };
    
    /// Retrieve the size of the overlap region in number of MCs.
    uint32_t overlap_size() const {
        return _overlap_size;
    };

    /// Retrieve the exp. size of the input node's data buffer in bytes.
    uint32_t in_data_buffer_size_exp() const {
        return _in_data_buffer_size_exp;
    };

    /// Retrieve the exp. size of the input node's descriptor buffer
    /// (number of entries).
    uint32_t in_desc_buffer_size_exp() const {
        return _in_desc_buffer_size_exp;
    };

    /// Retrieve the exp. size of the compute node's data buffer in bytes.
    uint32_t cn_data_buffer_size_exp() const {
        return _cn_data_buffer_size_exp;
    };

    /// Retrieve the exp. size of the compute node's descriptor buffer
    /// (number of entries).
    uint32_t cn_desc_buffer_size_exp() const {
        return _cn_desc_buffer_size_exp;
    };

    /// Retrieve the typical number of content bytes per MC.
    uint32_t typical_content_size() const {
        return _typical_content_size;
    };

    /// Retrieve the randomize sizes flag.
    bool randomize_sizes() const {
        return _randomize_sizes;
    };

    /// Retrieve the check pattern sizes flag.
    bool check_pattern() const {
        return _check_pattern;
    };

    /// Retrieve the global maximum timeslice number.
    uint32_t max_timeslice_number() const {
        return _max_timeslice_number;
    };

    /// Retrieve the name of the executable acting as timeslice processor.
    std::string processor_executable() const {
        return _processor_executable;
    };

    /// Retrieve the number of instances of the timeslice processor executable.
    uint32_t processor_instances() const {
        return _processor_instances;
    };

    /// Retrieve the global base port.
    uint32_t base_port() const {
        return _base_port;
    };
    
    /// Retrieve the number of completion queue entries.
    uint32_t num_cqe() const {
        return _num_cqe;
    };

    /// Retrieve the list of participating input nodes.
    std::vector<std::string> const input_nodes() const {
        return _input_nodes;
    };

    /// Retrieve the list of participating compute nodes.
    std::vector<std::string> const compute_nodes() const {
        return _compute_nodes;
    };
    
    /// Retrieve this applications's indexes in the list of input nodes.
    std::vector<unsigned> input_indexes() const {
        return _input_indexes;
    };

    /// Retrieve this applications's indexes in the list of compute nodes.
    std::vector<unsigned> compute_indexes() const {
        return _compute_indexes;
    };

private:
    /// Parse command line options.
    void parse_options(int argc, char* argv[]) {
        unsigned log_level = 3;

        po::options_description generic("Generic options");
        generic.add_options()
            ("version,V", "print version string")
            ("help,h", "produce help message")
            ("log-level,l", po::value<unsigned>(&log_level),
             "set the log level (default:3, all:0)")
            ;

        po::options_description config("Configuration");
        config.add_options()
            ("input-index,i", po::value< std::vector<unsigned> >()->multitoken(),
             "this application's index in the list of input nodes")
            ("compute-index,c", po::value< std::vector<unsigned> >()->multitoken(),
             "this application's index in the list of compute nodes")
            ("input-nodes,I",
             po::value< std::vector<std::string> >()->multitoken(),
             "add host to the list of input nodes")
            ("compute-nodes,C",
             po::value< std::vector<std::string> >()->multitoken(),
             "add host to the list of compute nodes")
            ("timeslice-size", po::value<uint32_t>(&_timeslice_size),
             "global timeslice size in number of MCs")
            ("overlap-size", po::value<uint32_t>(&_overlap_size),
             "size of the overlap region in number of MCs")
            ("in-data-buffer-size-exp",
             po::value<uint32_t>(&_in_data_buffer_size_exp),
             "exp. size of the input node's data buffer in bytes")
            ("in-desc-buffer-size-exp",
             po::value<uint32_t>(&_in_desc_buffer_size_exp),
             "exp. size of the input node's descriptor buffer"
             " (number of entries).")
            ("cn-data-buffer-size-exp", po::value<uint32_t>(&_cn_data_buffer_size_exp),
             "exp. size of the compute node's data buffer in bytes")
            ("cn-desc-buffer-size-exp", po::value<uint32_t>(&_cn_desc_buffer_size_exp),
             "exp. size of the compute node's descriptor buffer"
             " (number of entries).")
            ("typical-content-size", po::value<uint32_t>(&_typical_content_size),
             "typical number of content bytes per MC")
            ("randomize-sizes", po::value<bool>(&_randomize_sizes),
             "randomize sizes flag")
            ("check-pattern", po::value<bool>(&_check_pattern),
             "check pattern flag")
            ("max-timeslice-number", po::value<uint32_t>(&_max_timeslice_number),
             "global maximum timeslice number")
            ("processor-executable", po::value<std::string>(&_processor_executable),
             "name of the executable acting as timeslice processor")
            ("processor-instances", po::value<uint32_t>(&_processor_instances),
             "number of instances of the timeslice processor executable")
            ;

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
            std::cout << "flesnet, version 0.0" << "\n";
            exit(EXIT_SUCCESS);
        }

        if (!vm.count("input-nodes"))
            throw ParametersException("list of input nodes is empty");

        if (!vm.count("compute-nodes"))
            throw ParametersException("list of compute nodes is empty");

        _input_nodes = vm["input-nodes"].as< std::vector<std::string> >();
        _compute_nodes = vm["compute-nodes"].as< std::vector<std::string> >();

        if (vm.count("input-index"))
            _input_indexes = vm["input-index"].as< std::vector<unsigned> >();
        if (vm.count("compute-index"))
            _compute_indexes = vm["compute-index"].as< std::vector<unsigned> >();

        if (_input_nodes.empty() && _compute_nodes.empty()) {
            throw ParametersException("no node type specified");
        }

        for (auto input_index: _input_indexes) {
            if (input_index < 0 || input_index >= _input_nodes.size()) {
                std::ostringstream oss;
                oss << "input node index (" << input_index
                    << ") out of range (0.."
                    << _input_nodes.size() - 1 << ")";
                throw ParametersException(oss.str());
            }
        }

        for (auto compute_index: _compute_indexes) {
            if (compute_index < 0 || compute_index >= _compute_nodes.size()) {
                std::ostringstream oss;
                oss << "compute node index (" << compute_index
                    << ") out of range (0.."
                    << _compute_nodes.size() - 1 << ")";
                throw ParametersException(oss.str());
            }
        }

        out.setVerbosity(static_cast<einhard::LogLevel>(log_level));

        out.debug() << "input nodes (" << _input_nodes.size() << "): "
                    << _input_nodes;
        out.debug() << "compute nodes (" << _compute_nodes.size() << "): "
                    << _compute_nodes;
        for (auto input_index: _input_indexes) {
            out.info() << "this is input node " << input_index << " (of "
                       << _input_nodes.size() << ")";
        }
        for (auto compute_index: _compute_indexes) {
            out.info() << "this is compute node " << compute_index << " (of "
                       << _compute_nodes.size() << ")";
        }

        for (auto input_index: _input_indexes) {
            if (input_index == 0) {
                out.info() << "microslice size: " << _typical_content_size << " bytes";
                out.info() << "timeslice size: (" << _timeslice_size
                           << " + " << _overlap_size << ") microslices";
                out.info() << "number of timeslices: " << _max_timeslice_number;
                out.info() << "input node buffer size: (" << (1 << _in_data_buffer_size_exp) << " + "
                           <<  (1 << _in_desc_buffer_size_exp) * sizeof(MicrosliceDescriptor)
                           << ") bytes";
                out.info() << "compute node buffer size: ("
                           << (1 << _cn_data_buffer_size_exp) << " + "
                           << (1 << _cn_desc_buffer_size_exp) * sizeof(TimesliceComponentDescriptor)
                           << ") bytes";
            }
        }
    }

    /// The global timeslice size in number of MCs.
    uint32_t _timeslice_size = 100;

    /// The size of the overlap region in number of MCs.
    uint32_t _overlap_size = 2;

    /// The exp. size of the input node's data buffer in bytes.
    uint32_t _in_data_buffer_size_exp = 26;

    /// The exp. size of the input node's descriptor buffer (number of
    /// entries).
    uint32_t _in_desc_buffer_size_exp = 20;

    /// The exp. size of the compute node's data buffer in bytes.
    uint32_t _cn_data_buffer_size_exp = 17;

    /// The exp. size of the compute node's descriptor buffer (number of
    /// entries).
    uint32_t _cn_desc_buffer_size_exp = 6;

    /// A typical number of content bytes per MC.
    uint32_t _typical_content_size = 1024;

    /// The randomize sizes flag.
    bool _randomize_sizes = false;

    /// The check pattern sizes flag.
    bool _check_pattern = true;

    /// The global maximum timeslice number.
    uint32_t _max_timeslice_number = 100000;

    /// The name of the executable acting as timeslice processor.
    std::string _processor_executable;

    /// The number of instances of the timeslice processor executable.
    uint32_t _processor_instances = 2;

    /// The global base port.
    uint32_t _base_port = 20079;

    uint32_t _num_cqe = 1000000;

    /// The list of participating input nodes.
    std::vector<std::string> _input_nodes;

    /// The list of participating compute nodes.
    std::vector<std::string> _compute_nodes;

    /// This applications's indexes in the list of input nodes.
    std::vector<unsigned> _input_indexes;

    /// This applications's indexes in the list of compute nodes.
    std::vector<unsigned> _compute_indexes;
};
