// Copyright 2016 Helvi Hartmann

#include "EtcdClient.h"

EtcdClient::EtcdClient(std::string url_) : m_url(url_) {

    m_hnd = curl_easy_init();
}

std::string data; // how to do it differently?
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
    L_(info) << "waiting for " << make_address(prefix, key);
    curl_easy_setopt(m_hnd, CURLOPT_URL, make_address(prefix, key).c_str());
    curl_easy_setopt(m_hnd, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(m_hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(m_hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(m_hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
    return data;
}

enum Flags EtcdClient::check_value(Json::Value message) {
    Json::FastWriter fastwriter;
    Flags check_flag = errorneous;

    std::string value = fastwriter.write(message["node"]["value"]);
    std::string tag = fastwriter.write(message["node"]["modifiedIndex"]);
    // erase " from value
    value.erase(value.end() - 2, value.end());
    value.erase(0, 1);

    if (value == "on") {
        check_flag = ok;
    } else {
        m_requiredtag = stoi(tag) + 1;
        check_flag = notupdated;
    }

    return check_flag;
}

enum Flags EtcdClient::get_req(std::string prefix, std::string key) {
    std::string data;
    curl_easy_setopt(m_hnd, CURLOPT_URL, make_address(prefix, key).c_str());
    curl_easy_setopt(m_hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(m_hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(m_hnd);
    if (ret != CURLE_OK)
        L_(error) << "Get Request from key-value store error: "
                  << curl_easy_strerror(ret) << std::endl;
    Flags get_flag = parse_value(data);

    return get_flag;
}

int EtcdClient::check_process(std::string input_shm) {
    std::stringstream prefix;
    int ret;
    prefix << "/" << input_shm;
    ret = get_req(prefix.str(), "/uptodate");
    if (ret != 0) {
        L_(info) << "no shm set in key-value store...waiting";
        ret = wait_value(prefix.str());
        if (ret != 0) {
            L_(error) << "return flag was " << ret << ". Exiting";
            exit(EXIT_FAILURE);
        }
    }
    return ret;
}

void EtcdClient::set_value(std::string prefix, std::string key,
                           std::string value) {
    L_(info) << "Publishing " << value << " to " << make_address("", "")
             << prefix;
    curl_easy_setopt(m_hnd, CURLOPT_URL, make_address(prefix, key).c_str());
    curl_easy_setopt(m_hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(m_hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str()));
    curl_easy_setopt(m_hnd, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(m_hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(m_hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
}

std::string EtcdClient::make_address(std::string prefix, std::string key) {
    std::ostringstream address;
    address << m_url << prefix << key;

    return address.str();
}

enum Flags EtcdClient::parse_value(std::string data) {
    Json::Value message;
    Json::Reader reader;
    Json::FastWriter fastwriter;
    Flags parse_flag = errorneous;

    bool parsingSuccessful = reader.parse(data, message);
    if (!parsingSuccessful) {
        L_(warning) << "EtcdClient: Failed to parse";
        if (message.size() == 0) {
            L_(warning) << "EtcdClient: returned value from key-value store "
                           "was empty, waiting for updates";
            return parse_flag = empty;
        }
    }
    if (message.isMember("error")) {
        parse_flag = errorneous;
        L_(error) << fastwriter.write(message["error"]) << std::endl;
    } else
        parse_flag = check_value(message);

    return parse_flag;
}

int EtcdClient::wait_value(std::string prefix) {
    Flags wait_flag = empty;
    std::string data;
    std::ostringstream key_ss;
    key_ss << "/uptodate?wait=true&waitIndex=" << m_requiredtag;
    std::string key = key_ss.str();

    while (wait_flag == empty) {
        data = wait_req(prefix, key);
        wait_flag = parse_value(data);
    }
    return wait_flag;
}

void EtcdClient::delete_value(std::string prefix, std::string key) {
    curl_easy_setopt(m_hnd, CURLOPT_URL, make_address(prefix, key).c_str());
    curl_easy_setopt(m_hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(m_hnd, CURLOPT_CUSTOMREQUEST, "DELETE");

    CURLcode ret = curl_easy_perform(m_hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
}
