// Copyright 2016 Helvi Hartmann
// Copyright 2017 Jan de Cuveland <cmail@cuveland.de>

#include "log.hpp"
#include <cstring>
#include <curl/curl.h>
#include <iostream>
#include <json/json.h>
#include <json/reader.h>
#include <sstream>
#include <string>

enum Flags { ok, errorneous, empty, timeout, notexist };

typedef struct {
    std::string value;
    int tag;
} value_t;

class EtcdClient
{
public:
    EtcdClient(std::string url);

    bool set_value(const std::string key, const std::string value) const;
    bool get_value(const std::string key, std::string& value) const;
    bool delete_key(const std::string key) const;

    enum Flags check_process(std::string input_shm);

private:
    std::string m_url;

    std::pair<enum Flags, value_t> get_req(std::string key, bool wait = false);
    std::pair<enum Flags, value_t> parse_value(std::string data);
    enum Flags wait_value(std::string key, int requiredtag);
};
