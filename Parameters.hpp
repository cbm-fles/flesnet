/**
 * \file Parameters.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include "global.hpp"


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
    typedef enum {
        COMPUTE_NODE, ///< Node is a compute node.
        INPUT_NODE    ///< Node is an input node.
    } NodeType;

    /// The Parameters command-line parsing constructor.
    Parameters(int argc, char* argv[]) :
        _timesliceSize(100),
        _overlapSize(2),
        _inDataBufferSize(64 * 1024 * 1024),
        _inAddrBufferSize(1024 * 1024),
        _cnDataBufferSize(128 * 1024),
        _cnDescBufferSize(80),
        _typicalContentSize(128),
        _maxTimesliceNumber(1024 * 1024 * 1024),
        _basePort(20079)
    {
        parseOptions(argc, argv);
    };

    /// Enable use of (small) debug values for buffer sizes to enable
    /// manual buffer state analysis.
    void selectDebugValues() {
        _timesliceSize = 2;
        _overlapSize = 1;
        _inDataBufferSize = 32;
        _inAddrBufferSize = 10;
        _cnDataBufferSize = 32;
        _cnDescBufferSize = 4;
        _typicalContentSize = 2;
        _maxTimesliceNumber = 10;
    }

    /// Return a description of active nodes, suitable for debug output.
    std::string const desc() const {
        std::stringstream st;

        st << "input nodes (" << _inputNodes.size() << "): "
           << _inputNodes << std::endl;
        st << "compute nodes (" << _computeNodes.size() << "): "
           << _computeNodes << std::endl;
        st << "this node: "
           << (_nodeType == INPUT_NODE ? "input" : "compute")
           << " node #" << _nodeIndex;

        return st.str();
    };

    /// Retrieve the global timeslice size in number of MCs.
    uint32_t timesliceSize() const {
        return _timesliceSize;
    };
    
    /// Retrieve the size of the overlap region in number of MCs.
    uint32_t overlapSize() const {
        return _overlapSize;
    };

    /// Retrieve the size of the input node's data buffer in 64-bit words.
    uint32_t inDataBufferSize() const {
        return _inDataBufferSize;
    };

    /// Retrieve the size of the input node's address buffer in 64-bit words.
    uint32_t inAddrBufferSize() const {
        return _inAddrBufferSize;
    };

    /// Retrieve the size of the compute node's data buffer in 64-bit words.
    uint32_t cnDataBufferSize() const {
        return _cnDataBufferSize;
    };

    /// Retrieve the size of the compute node's description buffer
    /// (number of entries).
    uint32_t cnDescBufferSize() const {
        return _cnDescBufferSize;
    };

    /// Retrieve the typical number of content words per MC.
    uint32_t typicalContentSize() const {
        return _typicalContentSize;
    };

    /// Retrieve the global maximum timeslice number.
    uint32_t maxTimesliceNumber() const {
        return _maxTimesliceNumber;
    };

    /// Retrieve the global base port.
    uint32_t basePort() const {
        return _basePort;
    };
    
    /// Retrieve the list of participating input nodes.
    std::vector<std::string> const inputNodes() const {
        return _inputNodes;
    };

    /// Retrieve the list of participating compute nodes.
    std::vector<std::string> const computeNodes() const {
        return _computeNodes;
    };
    
    /// Retrieve this node's type.
    NodeType nodeType() const {
        return _nodeType;
    };

    /// Retrieve this node's index in the list of nodes of same type.
    unsigned nodeIndex() const {
        return _nodeIndex;
    };

private:

    /// The global timeslice size in number of MCs.
    uint32_t _timesliceSize;

    /// The size of the overlap region in number of MCs.
    uint32_t _overlapSize;

    /// The size of the input node's data buffer in 64-bit words.
    uint32_t _inDataBufferSize;

    /// The size of the input node's address buffer in 64-bit words.
    uint32_t _inAddrBufferSize;

    /// The size of the compute node's data buffer in 64-bit words.
    uint32_t _cnDataBufferSize;

    /// The size of the compute node's description buffer (number of
    /// entries).
    uint32_t _cnDescBufferSize;

    /// A typical number of content words per MC.
    uint32_t _typicalContentSize;

    /// The global maximum timeslice number.
    uint32_t _maxTimesliceNumber;

    /// The global base port.
    uint32_t _basePort;

    /// The list of participating input nodes.
    std::vector<std::string> _inputNodes;
    
    /// The list of participating compute nodes.
    std::vector<std::string> _computeNodes;

    /// This node's type.
    NodeType _nodeType;

    /// This node's index in the list of nodes of same type.
    unsigned _nodeIndex;

    /// Parse command line options.
    void parseOptions(int argc, char* argv[]) {
        unsigned logLevel = 3;

        po::options_description generic("Generic options");
        generic.add_options()
            ("version,V", "print version string")
            ("help,h", "produce help message")
            ("log-level,l", po::value<unsigned>(&logLevel),
             "set the log level (default:3, all:0)")
            ;

        po::options_description config("Configuration");
        config.add_options()
            ("input-node,i", po::value<unsigned>(&_nodeIndex),
             "act as input node with given index")
            ("compute-node,c", po::value<unsigned>(&_nodeIndex),
             "act as compute node with given index")
            ("input-nodes,I",
             po::value< std::vector<std::string> >()->multitoken(),
             "add host to the list of input nodes")
            ("compute-nodes,C",
             po::value< std::vector<std::string> >()->multitoken(),
             "add host to the list of compute nodes")
            ;

        po::options_description cmdline_options("Allowed options");
        cmdline_options.add(generic).add(config);

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        std::ifstream ifs("ibtsb.cfg");
        po::store(po::parse_config_file(ifs, config), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << cmdline_options << "\n";
            exit(EXIT_SUCCESS);
        }

        if (vm.count("version")) {
            std::cout << "ibtsb, version 0.0" << "\n";
            exit(EXIT_SUCCESS);
        }

        if (!vm.count("input-nodes"))
            throw ParametersException("list of input nodes is empty");

        if (!vm.count("compute-nodes"))
            throw ParametersException("list of compute nodes is empty");

        _inputNodes = vm["input-nodes"].as< std::vector<std::string> >();
        _computeNodes = vm["compute-nodes"].as< std::vector<std::string> >();

        if (vm.count("input-node")) {
            if (vm.count("compute-node")) {
                throw ParametersException("more than one node type specifed");
            } else {
                if (_nodeIndex < 0 || _nodeIndex >= _inputNodes.size()) {
                    std::ostringstream oss;
                    oss << "node index (" << _nodeIndex
                        << ") out of range (0.."
                        << _inputNodes.size() - 1 << ")";
                    throw ParametersException(oss.str());
                }
                _nodeType = INPUT_NODE;
            }
        } else {
            if (vm.count("compute-node")) {
                if (_nodeIndex < 0 || _nodeIndex >= _computeNodes.size()) {
                    std::ostringstream oss;
                    oss << "node index (" << _nodeIndex
                        << ") out of range (0.."
                        << _computeNodes.size() - 1 << ")";
                    throw ParametersException(oss.str());
                }
                _nodeType = COMPUTE_NODE;
            } else {
                throw ParametersException("no node type specifed");
            }
        }

        Log.setVerbosity((einhard::LogLevel)logLevel);

        Log.debug() << "input nodes (" << _inputNodes.size() << "): "
                    << _inputNodes;
        Log.debug() << "compute nodes (" << _computeNodes.size() << "): "
                    << _computeNodes;
        Log.debug() << "this node: "
                    << (_nodeType == INPUT_NODE ? "input" : "compute")
                    << " node #" << _nodeIndex;
    }
};


#endif /* PARAMETERS_HPP */
