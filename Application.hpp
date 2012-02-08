/*
 * Application.hpp
 *
 * 2012, Jan de Cuveland
 */
 
#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "Parameters.hpp"


class Application
{
public:
    explicit Application(Parameters& par) : _par(par) { };
    virtual int run() = 0;
protected:
    Parameters& _par;
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


#endif /* IBTSB_HPP */
