/**
 * \file Parameters.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

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

    /// The function of a node in timeslice building.
    enum NodeType {
        COMPUTE_NODE, ///< Node is a compute node.
        INPUT_NODE    ///< Node is an input node.
    };

    /// The Parameters command-line parsing constructor.
    Parameters(int argc, char* argv[]) {
        parse_options(argc, argv);
    };

    /// Return a description of active nodes, suitable for debug output.
    std::string const desc() const {
        std::stringstream st;

        st << "input nodes (" << _input_nodes.size() << "): "
           << _input_nodes << std::endl;
        st << "compute nodes (" << _compute_nodes.size() << "): "
           << _compute_nodes << std::endl;
        st << "this node: "
           << (_node_type == INPUT_NODE ? "input" : "compute")
           << " node #" << _node_index;

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

    /// Retrieve the exp. size of the input node's data buffer in 64-bit words.
    uint32_t in_data_buffer_size_exp() const {
        return _in_data_buffer_size_exp;
    };

    /// Retrieve the exp. size of the input node's address buffer in
    /// 64-bit words.
    uint32_t in_addr_buffer_size_exp() const {
        return _in_addr_buffer_size_exp;
    };

    /// Retrieve the exp. size of the compute node's data buffer in 64-bit words.
    uint32_t cn_data_buffer_size_exp() const {
        return _cn_data_buffer_size_exp;
    };

    /// Retrieve the size of the compute node's description buffer
    /// (number of entries).
    uint32_t cn_desc_buffer_size_exp() const {
        return _cn_desc_buffer_size_exp;
    };

    /// Retrieve the typical number of content words per MC.
    uint32_t typical_content_size() const {
        return _typical_content_size;
    };

    /// Retrieve the randomize sizes flag.
    bool randomize_sizes() const {
        return _randomize_sizes;
    };

    /// Retrieve the global maximum timeslice number.
    uint32_t max_timeslice_number() const {
        return _max_timeslice_number;
    };

    /// Retrieve the global base port.
    uint32_t base_port() const {
        return _base_port;
    };
    
    /// Retrieve the list of participating input nodes.
    std::vector<std::string> const input_nodes() const {
        return _input_nodes;
    };

    /// Retrieve the list of participating compute nodes.
    std::vector<std::string> const compute_nodes() const {
        return _compute_nodes;
    };
    
    /// Retrieve this node's type.
    NodeType node_type() const {
        return _node_type;
    };

    /// Retrieve this node's index in the list of nodes of same type.
    unsigned node_index() const {
        return _node_index;
    };

private:

    /// The global timeslice size in number of MCs.
    uint32_t _timeslice_size = 100;

    /// The size of the overlap region in number of MCs.
    uint32_t _overlap_size = 2;

    /// The exp. size of the input node's data buffer in 64-bit words.
    uint32_t _in_data_buffer_size_exp = 26;

    /// The exp. size of the input node's address buffer in 64-bit words.
    uint32_t _in_addr_buffer_size_exp = 20;

    /// The exp. size of the compute node's data buffer in 64-bit words.
    uint32_t _cn_data_buffer_size_exp = 17;

    /// The exp. size of the compute node's description buffer (number of
    /// entries).
    uint32_t _cn_desc_buffer_size_exp = 6;

    /// A typical number of content words per MC.
    uint32_t _typical_content_size = 128;

    /// The randomize sizes flag.
    bool _randomize_sizes = false;

    /// The global maximum timeslice number.
    uint32_t _max_timeslice_number = 100000;

    /// The global base port.
    uint32_t _base_port = 20079;

    /// The list of participating input nodes.
    std::vector<std::string> _input_nodes;
    
    /// The list of participating compute nodes.
    std::vector<std::string> _compute_nodes;

    /// This node's type.
    NodeType _node_type;

    /// This node's index in the list of nodes of same type.
    unsigned _node_index;

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
            ("input-node,i", po::value<unsigned>(&_node_index),
             "act as input node with given index")
            ("compute-node,c", po::value<unsigned>(&_node_index),
             "act as compute node with given index")
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
             "exp. size of the input node's data buffer in 64-bit words")
             ("in-addr-buffer-size-exp",
              po::value<uint32_t>(&_in_addr_buffer_size_exp),
             "exp. size of the input node's address buffer in 64-bit words")
            ("cn-data-buffer-size-exp", po::value<uint32_t>(&_cn_data_buffer_size_exp),
             "exp. size of the compute node's data buffer in 64-bit words")
            ("cn-desc-buffer-size-exp", po::value<uint32_t>(&_cn_desc_buffer_size_exp),
             "exp. size of the compute node's description buffer"
             " (number of entries).")
            ("typical-content-size", po::value<uint32_t>(&_typical_content_size),
             "typical number of content words per MC")
            ("randomize-sizes", po::value<bool>(&_randomize_sizes),
             "randomize sizes flag")
            ("max-timeslice-number", po::value<uint32_t>(&_max_timeslice_number),
             "global maximum timeslice number")
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

        if (vm.count("input-node")) {
            if (vm.count("compute-node")) {
                throw ParametersException("more than one node type specifed");
            } else {
                if (_node_index < 0 || _node_index >= _input_nodes.size()) {
                    std::ostringstream oss;
                    oss << "node index (" << _node_index
                        << ") out of range (0.."
                        << _input_nodes.size() - 1 << ")";
                    throw ParametersException(oss.str());
                }
                _node_type = INPUT_NODE;
            }
        } else {
            if (vm.count("compute-node")) {
                if (_node_index < 0 || _node_index >= _compute_nodes.size()) {
                    std::ostringstream oss;
                    oss << "node index (" << _node_index
                        << ") out of range (0.."
                        << _compute_nodes.size() - 1 << ")";
                    throw ParametersException(oss.str());
                }
                _node_type = COMPUTE_NODE;
            } else {
                throw ParametersException("no node type specifed");
            }
        }

        out.setVerbosity((einhard::LogLevel)log_level);

        out.debug() << "input nodes (" << _input_nodes.size() << "): "
                    << _input_nodes;
        out.debug() << "compute nodes (" << _compute_nodes.size() << "): "
                    << _compute_nodes;
        out.info() << "this is "
                   << (_node_type == INPUT_NODE ? "input" : "compute")
                   << " node " << _node_index << " (of "
                   << (_node_type == INPUT_NODE ? _input_nodes.size() :  _compute_nodes.size())
                   << ")";

        if (_node_type == INPUT_NODE && _node_index == 0) {
            out.info() << "microslice size: ("
                       << _typical_content_size * sizeof(uint64_t)
                       << " + " << 2 * sizeof(uint64_t) << ") bytes";
            out.info() << "timeslice size: (" << _timeslice_size
                       << " + " << _overlap_size << ") microslices";
            out.info() << "number of timeslices: " << _max_timeslice_number;
            out.info() << "input node buffer size: ("
                       << (1 << _in_data_buffer_size_exp) * sizeof(uint64_t)
                       << " + " <<  (1 << _in_addr_buffer_size_exp) * sizeof(uint64_t)
                       << ") bytes";
            out.info() << "compute node buffer size: ("
                       << (1 << _cn_data_buffer_size_exp) * sizeof(uint64_t) << " + "
                       << (1 << _cn_desc_buffer_size_exp) * sizeof(TimesliceComponentDescriptor)
                       << ") bytes";
        }
    }
};


#endif /* PARAMETERS_HPP */
