#include <iostream>
#include <string>
#include <curl/curl.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include "log.hpp"

using namespace std;

class HttpClient{
private:
    string url;
    CURL *hnd;


public:
    
    HttpClient(string url_);
    
    ~HttpClient();
    
    string setadress(string prefix, string key);
    int putreq(string prefix, string key, string value, string method);
    int waitreq(string prefix, string key);
    string getreq(string prefix, string key);
};
