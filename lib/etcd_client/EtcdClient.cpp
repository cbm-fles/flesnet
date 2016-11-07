// Copyright 2016 Helvi Hartmann

#include "EtcdClient.h"

EtcdClient::EtcdClient(std::string url_) : m_url(url_) {}

size_t write_callback(char* buf, size_t size, size_t nmemb, void* userdata) {
    // callback must have this declaration
    std::string* data = reinterpret_cast<std::string*>(userdata);
    data->append(buf, size * nmemb);
    return size * nmemb;
}

std::pair<enum Flags, value_t>
EtcdClient::get_req(std::string prefix, std::string key, bool timeout) {
    std::string data;
    CURL* hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, (m_url + prefix + key).c_str());
    if (timeout) {
        L_(info) << "waiting for " << m_url << prefix << key;
        curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 8000L);
    }
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(hnd);
    std::pair<enum Flags, value_t> returnvalue(errorneous, {"", 1});
    if (ret == CURLE_OK)
        returnvalue = parse_value(data);
    else
        L_(error) << curl_easy_strerror(ret) << std::endl;

    curl_easy_cleanup(hnd);

    return returnvalue;
}

int EtcdClient::set_value(std::string prefix, std::string key,
                          std::string value) {
    L_(info) << "Publishing " << value << " to " << m_url << prefix;
    CURL* hnd = curl_easy_init();
    std::string data;
    curl_easy_setopt(hnd, CURLOPT_URL, (m_url + prefix + key).c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str()));
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
    curl_easy_cleanup(hnd);

    // if 0 ok, else error
    return ret;
}

enum Flags EtcdClient::wait_value(std::string prefix, int requiredtag) {
    Flags wait_flag = empty;

    while (wait_flag != ok) {
        std::pair<enum Flags, value_t> returnvalue = get_req(
            prefix, "?wait=true&waitIndex=" + std::to_string(requiredtag),
            true);
        if (returnvalue.second.value == "on") wait_flag = ok;
    }
    return wait_flag;
}

std::pair<enum Flags, value_t> EtcdClient::parse_value(std::string data) {
    Json::Value message;
    Json::Reader reader;
    reader.parse(data, message);
    std::pair<enum Flags, value_t> returnvalue(errorneous, {"", 1});

    if (message.empty()) {
        L_(warning) << "EtcdClient: returned value from key-value store "
                       "was empty, waiting for updates";
        returnvalue.first = empty;
        return returnvalue;
    }

    if (message.isMember("errorCode")) {
        L_(error) << message["message"].asString() << std::endl;
    } else {
        returnvalue.first = ok;
        returnvalue.second.value = message["node"]["value"].asString();
        returnvalue.second.tag = message["node"]["modifiedIndex"].asInt();
    }

    return returnvalue;
}

int EtcdClient::check_process(std::string input_shm) {
    std::pair<enum Flags, value_t> returnvalue =
        get_req("/" + input_shm, "/uptodate", false);
    if (returnvalue.first != errorneous) {
        if (returnvalue.second.value == "on")
            return ok;
        else {
            L_(info) << "no shm set in key-value store...waiting";
            returnvalue.first = wait_value("/" + input_shm + "/uptodate",
                                           returnvalue.second.tag + 1);
        }
    }

    return returnvalue.first;
}
