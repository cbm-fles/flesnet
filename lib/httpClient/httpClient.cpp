#include "httpClient.hpp"
#include <sstream>
#include <cstring>

using namespace std;


HttpClient::HttpClient(string url_)
    :url(url_)
{
     hnd = curl_easy_init();
}

string HttpClient::setadress(string prefix, string key){
    ostringstream adress;
    adress << url << prefix << key;
    
    return adress.str();
}

int printerror(CURLcode ret){
    int flag = 2;
    if( ret != 0){
        L_(error) << curl_easy_strerror(ret) << endl;
        flag = 0;
    }
    else flag = 2;
    
    return flag;
}

int HttpClient::putreq(string prefix, string key, string value, string method){
    cout << setadress(prefix, key) << endl;
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str()));
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

    int flag = printerror(curl_easy_perform(hnd));

    return flag;
}

int HttpClient::deletereq(string prefix, string key){
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.38.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_SSH_KNOWNHOSTS, "/home/hartmann/.ssh/known_hosts");
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    
    int flag = printerror(curl_easy_perform(hnd));
    
    return flag;
}

int HttpClient::waitreq(string prefix, string key){
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix,key).c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    
    cout << setadress(prefix,key) << endl;
    int flag = printerror(curl_easy_perform(hnd));
    if (flag == 2) flag = 0;//not clear yet but if more than one uptade has occured, prints out list and error 2
    return flag;
}

string HttpClient::getreq(string prefix, string key){
    ostringstream os;
    os << curlpp::options::Url(setadress(prefix,key));
    return os.str();
}

HttpClient::~HttpClient(){
    curl_easy_cleanup(hnd);
    hnd = NULL;
    cout << "curl handle destroyed" << endl;
    
}
