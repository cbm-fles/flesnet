// Copyright 2016 Helvi Hartmann

#include "log.hpp"
#include <cstring>
#include <curl/curl.h>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <sstream>
#include <string>

enum Flags { ok, notupdated, errorneous, empty };

class EtcdClient {
private:
    std::string m_url;

    std::string wait_req(std::string prefix, std::string key);

    std::pair <enum Flags, int> check_value(Json::Value message);
    
    std::pair <enum Flags, int> parse_value(std::string data);


    std::pair <enum Flags, int> get_req(std::string prefix, std::string key);

    std::string make_address(std::string prefix, std::string key);


    int wait_value(std::string prefix);

public:
    EtcdClient(std::string url);

    int check_process(std::string input_shm);

    void set_value(std::string prefix, std::string key, std::string value);
};
