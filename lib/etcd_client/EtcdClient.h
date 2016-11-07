// Copyright 2016 Helvi Hartmann

#include "log.hpp"
#include <cstring>
#include <curl/curl.h>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <sstream>
#include <string>

enum Flags { ok, errorneous, empty };
typedef struct {
    std::string value;
    int tag;
} value_t;

class EtcdClient {
private:
    std::string m_url;

    std::pair<enum Flags, value_t> get_req(std::string prefix, std::string key,
                                           bool timeout = false);

    std::pair<enum Flags, value_t> parse_value(std::string data);

    enum Flags wait_value(std::string prefix, int requiredtag);

public:
    EtcdClient(std::string url);

    int check_process(std::string input_shm);

    int set_value(std::string prefix, std::string key, std::string value);
};
