// Copyright 2016 Helvi Hartmann

#include "HttpClient.hpp"
#include <cstring>
#include <sstream>

using namespace std;

HttpClient::HttpClient(string url_) : url(url_) { hnd = curl_easy_init(); }

string HttpClient::setadress(string prefix, string key) {
    ostringstream adress;
    adress << url << prefix << key;

    return adress.str();
}

int printerror(CURLcode ret) {
    int flag = 2;
    if (ret != 0) {
        L_(error) << curl_easy_strerror(ret) << endl;
        flag = 0;
    } else
        flag = 2;

    return flag;
}

string data;

size_t writeCallback(char* buf, size_t size, size_t nmemb, void* up) {
    // callback must have this declaration
    // buf is a pointer to the data that curl has for us
    // size*nmemb is the size of the buffer

    (void)up;
    for (unsigned int c = 0; c < size * nmemb; c++) {
        data.push_back(buf[c]);
    }
    return size * nmemb; // tell curl how many bytes we handled
}

int HttpClient::putreq(string prefix, string key, string value, string method) {
    L_(info) << "Publishing " << value << " to " << setadress("", "") << prefix;
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str()));
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &writeCallback);

    int flag = 0;
    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) {
        printerror(ret);
        flag = 2;
    }
    return flag;
}

int HttpClient::deletereq(string prefix, string key) {
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.38.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

    int flag = 0;
    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) {
        printerror(ret);
        flag = 2;
    }
    return flag;
}

string HttpClient::waitreq(string prefix, string key) {
    data.clear();
    L_(info) << "waiting for " << setadress(prefix, key);
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &writeCallback);

    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) printerror(ret);
    return data;
}

string HttpClient::getreq(string prefix, string key) {
    data.clear();
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &writeCallback);

    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) printerror(ret);
    return data;
}

HttpClient::~HttpClient() {
    curl_easy_cleanup(hnd);
    hnd = NULL;
}
