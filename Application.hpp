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


enum REQUEST_ID { ID_WRITE_DATA = 1, ID_WRITE_DATA_WRAP, ID_WRITE_DESC,
                  ID_SEND_CN_WP, ID_RECEIVE_CN_ACK
                };

#define ACK_WORDS (ADDR_WORDS / TS_SIZE + 1) // TODO: clean up

enum { RESOLVE_TIMEOUT_MS = 5000 };

#define BUFDEBUG
#ifndef BUFDEBUG
enum {
    TS_SIZE = 100, // timeslice size in number of MCs
    TS_OVERLAP = 2, // overlap region in number of MCs
    DATA_WORDS = 64 * 1024 * 1024, // data buffer in 64-bit words
    ADDR_WORDS = 1024 * 1024, // address buffer in 64-bit words
    CN_DATABUF_WORDS = 128 * 1024, // data buffer in 64-bit words
    CN_DESCBUF_WORDS = 80, // desc buffer in entries
    TYP_CNT_WORDS = 128, // typical content words in MC
    NUM_TS = 1024 * 1024 * 1024
};
#else
enum {
    TS_SIZE = 2, // timeslice size in number of MCs
    TS_OVERLAP = 1, // overlap region in number of MCs
    DATA_WORDS = 32, // data buffer in 64-bit words
    ADDR_WORDS = 10, // address buffer in 64-bit words
    CN_DATABUF_WORDS = 32, // data buffer in 64-bit words
    CN_DESCBUF_WORDS = 4, // desc buffer in entries
    TYP_CNT_WORDS = 2, // typical content words in MC
    NUM_TS = 10
};
#endif

#define BASE_PORT 20079


inline std::ostream& operator<<(std::ostream& s, REQUEST_ID v)
{
    switch (v) {
    case ID_WRITE_DATA:
        return s << "ID_WRITE_DATA";
    case ID_WRITE_DATA_WRAP:
        return s << "ID_WRITE_DATA_WRAP";
    case ID_WRITE_DESC:
        return s << "ID_WRITE_DESC";
    case ID_SEND_CN_WP:
        return s << "ID_SEND_CN_WP";
    case ID_RECEIVE_CN_ACK:
        return s << "ID_RECEIVE_CN_ACK";
    default:
        return s << (int) v;
    }
}


class ApplicationException : public std::runtime_error {
public:
    explicit ApplicationException(const std::string& what_arg = "")
        : std::runtime_error(what_arg) { }
};


class Application {
public:
    typedef struct {
        uint64_t buf_va;
        uint32_t buf_rkey;
    } pdata_t;
    explicit Application(Parameters const& par) : _par(par) { };
    virtual int run() = 0;
protected:
    Parameters const& _par;
};


class InputApplication : public Application {
public:
    explicit InputApplication(Parameters& par) : Application(par) { };
    virtual int run();
};


class ComputeApplication : public Application {
public:
    explicit ComputeApplication(Parameters& par) : Application(par) { };
    virtual int run();
};


#endif /* APPLICATION_HPP */
