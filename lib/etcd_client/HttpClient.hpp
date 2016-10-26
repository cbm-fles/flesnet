// Copyright 2016 Helvi Hartmann

#include "log.hpp"
#include <curl/curl.h>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <iostream>
#include <string>

using namespace std;

class HttpClient {
private:
    string url;
    CURL* hnd;

public:
    HttpClient(string url_);

    ~HttpClient();

    string setadress(string prefix, string key);
    int putreq(string prefix, string key, string value, string method);
    int deletereq(string prefix, string key);
    string waitreq(string prefix, string key);
    string getreq(string prefix, string key);
};
