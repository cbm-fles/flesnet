// Copyright 2016 Helvi Hartmann

#include "EtcdClient.h"

EtcdClient::EtcdClient(std::string url_) : m_url(url_) {}

size_t write_callback(char* buf, size_t size, size_t nmemb, void* userdata) {
    // callback must have this declaration
    // buf is a pointer to the data that curl has for us
    // size*nmemb is the size of the buffer
    std::string* data = reinterpret_cast<std::string*>(userdata);
    for (unsigned int c = 0; c < size * nmemb; c++) {
        data->push_back(buf[c]);
    }
    return size * nmemb; // tell curl how many bytes we handled
}

std::string EtcdClient::wait_req(std::string prefix, std::string key) {
    std::string data;
    L_(info) << "waiting for " << m_url << prefix << key;
    CURL* hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, (m_url + prefix + key).c_str());
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
    curl_easy_cleanup(hnd);
    return data;
}

int EtcdClient::set_value(std::string prefix, std::string key,
                          std::string value) {
    L_(info) << "Publishing " << value << " to " << m_url << prefix;
    CURL* hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, (m_url + prefix + key).c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str()));
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
    curl_easy_cleanup(hnd);

    return ret;
}

std::pair<enum Flags, int> EtcdClient::get_req(std::string prefix,
                                               std::string key) {
    std::string data;
    CURL* hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, (m_url + prefix + key).c_str());
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(hnd);

    if (ret != CURLE_OK) {
        L_(error) << "Get Request from key-value store error: ";
        L_(error) << curl_easy_strerror(ret) << std::endl;
    }

    std::pair<enum Flags, int> returnvalue = parse_value(data);
    curl_easy_cleanup(hnd);

    return returnvalue;
}

std::pair<enum Flags, int> EtcdClient::parse_value(std::string data) {
    Json::Value message(data);
    std::pair<enum Flags, int> returnvalue;
    returnvalue = std::make_pair(errorneous, 1);

    if (message.size() == 0) {
        L_(warning) << "EtcdClient: returned value from key-value store "
                       "was empty, waiting for updates";
        returnvalue.first = empty;
        return returnvalue;
    }

    if (message.isMember("errorCode")) {
        L_(error) << message["message"].asString() << std::endl;
    } else
        returnvalue = check_value(message);

    return returnvalue;
}

std::pair<enum Flags, int> EtcdClient::check_value(Json::Value message) {
    enum Flags check_flag = errorneous;
    int requiredtag = 1;
    std::string value = message["node"]["value"].asString();
    int tag = message["node"]["modifiedIndex"].asInt();

    if (value == "on") {
        check_flag = ok;
    } else {
        requiredtag = tag + 1;
        check_flag = notupdated;
    }

    return std::make_pair(check_flag, requiredtag);
}

int EtcdClient::check_process(std::string input_shm) {
    enum Flags ret = get_req("/" + input_shm, "/uptodate").first;
    if (ret != ok) {
        L_(info) << "no shm set in key-value store...waiting";
        ret = wait_value("/" + input_shm);
        if (ret == errorneous) {
            L_(error) << "return flag was " << ret << ". Exiting";
        }
    }
    return ret;
}

enum Flags EtcdClient::wait_value(std::string prefix) {
    Flags wait_flag = empty;
    int requiredtag = 1;

    while ((wait_flag == empty) || (wait_flag == notupdated)) {
        std::string data = wait_req(prefix, "/uptodate?wait=true&waitIndex=" +
                                                std::to_string(requiredtag));
        std::pair<enum Flags, int> returnvalue = parse_value(data);
        wait_flag = returnvalue.first;
        requiredtag = returnvalue.second;
    }
    return wait_flag;
}
