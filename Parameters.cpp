/*
 * Parameters.cpp
 *
 * 2012, Jan de Cuveland
 */

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>

#include "Parameters.hpp"
#include "log.hpp"

void
Parameters::parse_options(int argc, char* argv[])
{
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

    _input_nodes = vm["input-nodes"].as< std::vector<std::string> >();
    _compute_nodes = vm["compute-nodes"].as< std::vector<std::string> >();

    if (vm.count("input-node")) {
        if (vm.count("compute-node")) {
            throw ParametersException("more than one node type specifed");
        } else {
            if (_node_index < 0 || _node_index >= _input_nodes.size()) {
                std::ostringstream oss;
                oss << "node index (" << _node_index << ") out of range (0.."
                    << _input_nodes.size() - 1 << ")";
                throw ParametersException(oss.str());
            }
            _node_type = INPUT_NODE;
        }
    } else {
        if (vm.count("compute-node")) {
            if (_node_index < 0 || _node_index >= _compute_nodes.size()) {
                std::ostringstream oss;
                oss << "node index (" << _node_index << ") out of range (0.."
                    << _compute_nodes.size() - 1 << ")";
                throw ParametersException(oss.str());
            }
            _node_type = COMPUTE_NODE;
        } else {
            throw ParametersException("no node type specifed");
        }
    }

    Log.setVerbosity((einhard::LogLevel)log_level);

    Log.debug() << "input nodes (" << _input_nodes.size() << "): "
                << _input_nodes;
    Log.debug() << "compute nodes (" << _compute_nodes.size() << "): "
                << _compute_nodes;
    Log.debug() << "this node: "
                << (_node_type == INPUT_NODE ? "input" : "compute")
                << " node #" << _node_index;
}


template<class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}


std::string const
Parameters::desc() const
{
    std::stringstream st;

    st << "input nodes (" << _input_nodes.size() << "): "
       << _input_nodes << std::endl;
    st << "compute nodes (" << _compute_nodes.size() << "): "
       << _compute_nodes << std::endl;
    st << "this node: "
       << (_node_type == INPUT_NODE ? "input" : "compute")
       << " node #" << _node_index;

    return st.str();
}
