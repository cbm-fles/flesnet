// Copyright 2016 Helvi Hartmann

#include "log.hpp"
#include <cstring>
#include <curl/curl.h>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <sstream>
#include <string>

class EtcdClient {
private:
    std::string m_url;
    CURL* m_hnd;
    int m_requiredtag = 1;
    std::string m_data;

public:
    EtcdClient(std::string url);

    std::string wait_req(std::string prefix, std::string key);

    int check_value(Json::Value message);

    int get_req(std::string prefix, std::string key);

    int check_process(std::string input_shm);

    std::string make_address(std::string prefix, std::string key);

    void set_value(std::string prefix, std::string key, std::string value);

    int parse_value(std::string data);

    int wait_value(std::string prefix);

    void delete_value(std::string prefix, std::string key);
};
