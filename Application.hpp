/*
 * Application.hpp
 *
 * 2012, Jan de Cuveland
 */
 
#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <iostream>
#include <boost/cstdint.hpp>

#include "Parameters.hpp"


class ApplicationException : public std::runtime_error {
public:
    explicit ApplicationException(const std::string& what_arg = "")
        : std::runtime_error(what_arg) { }
};


class Application
{
public:
    typedef struct {
        uint64_t buf_va;
        uint32_t buf_rkey;
    } pdata_t;
    explicit Application(Parameters const& par) : _par(par) { };
    virtual int run() = 0;
protected:
    void DEBUG(std::string const& s) const {
        if (_par.verbose())
            std::cout << "Debug: " << s << std::endl;
    }
    Parameters const& _par;
};


class InputApplication : public Application
{
public:
    explicit InputApplication(Parameters& par) : Application(par) { };
    virtual int run();
private:
    enum { RESOLVE_TIMEOUT_MS = 5000 };
};


class ComputeApplication : public Application
{
public:
    explicit ComputeApplication(Parameters& par) : Application(par) { };
    virtual int run();
};


#endif /* IBTSB_HPP */
