// Copyright 2016 Helvi Hartmann

#include "HttpClient.hpp"
#include "log.hpp"
#include <cstring>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <sstream>
#include <string>

using namespace std;

class EtcdClient {
private:
    string url;
    HttpClient http;
    // CURL *hnd;
    // CURLcode ret;
    string answer;
    string value;
    int requiredtag = 1;

public:
    EtcdClient(string url);

    int checkonprocess(string input_shm);

    string setadress(string prefix, string key);

    void setvalue(string prefix, string key, string value) {
        http.putreq(prefix, key, value, "PUT");
    }

    void deletevalue(string prefix, string key) { http.deletereq(prefix, key); }

    int getvalue(string prefix, string key);

    int parsevalue(string data);

    int checkvalue(Json::Value message);

    int waitvalue(string prefix);

    string getanswer() { return answer; }
};
