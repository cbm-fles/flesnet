#include <curl/curl.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>
#include <string>
#include <iostream>
#include <sstream>
#include <cstring>

using namespace std;

class EtcdClient{
private:
    CURL *hnd;
    CURLcode ret;
    string url;
    string answer;
    
public:
    EtcdClient(string url);
    
    EtcdClient();
    
    string setadress(string prefix, string key);
    
    void setvalue(string prefix, string key, string value);
    
    int getvalue(string key, bool  uptodate);
    
    int waitvalue(string prefix);
    
    string getanswer() { return answer; }
    
};