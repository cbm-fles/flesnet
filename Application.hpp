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


// timeslice component descriptor
typedef struct {
    uint64_t ts_num;
    uint64_t offset;
    uint64_t size;
} tscdesc_t;


typedef struct {
    uint64_t data;
    uint64_t desc;
} cn_bufpos_t;

typedef struct {
    uint8_t hdrrev;
    uint8_t sysid;
    uint16_t flags;
    uint32_t size;
    uint64_t time;
} mc_hdr_t;


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
};


class ComputeApplication : public Application
{
public:
    explicit ComputeApplication(Parameters& par) : Application(par) { };
    virtual int run();
};


#endif /* APPLICATION_HPP */
