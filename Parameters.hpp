/*
 * Parameters.hpp
 *
 * 2012, Jan de Cuveland
 */

#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

#include <vector>
#include <string>
#include <stdexcept>


class ParametersException : public std::runtime_error {
public:
    explicit ParametersException(const std::string& what_arg = "")
        : std::runtime_error(what_arg) { }
};


class Parameters {
public:
    typedef enum { COMPUTE_NODE, INPUT_NODE } node_type_t;

    Parameters() { };
    Parameters(int argc, char* argv[]) {
        parse_options(argc, argv);
    };

    std::string const desc() const;
    std::vector<std::string> const input_nodes() const {
        return _input_nodes;
    };
    std::vector<std::string> const compute_nodes() const {
        return _compute_nodes;
    };
    node_type_t node_type() const {
        return _node_type;
    };
    unsigned node_index() const {
        return _node_index;
    };
    bool verbose() const {
        return _verbose;
    };

private:
    void parse_options(int argc, char* argv[]);

private:
    std::vector<std::string> _input_nodes;
    std::vector<std::string> _compute_nodes;
    node_type_t _node_type;
    unsigned _node_index;
    bool _verbose;
};


#endif /* PARAMETERS_HPP */
