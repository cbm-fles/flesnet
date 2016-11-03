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
    L_(info) << "waiting for " << make_address(prefix, key);
    CURL *hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, make_address(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);

    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
    curl_easy_cleanup(hnd);
    return data;
}

void EtcdClient::set_value(std::string prefix, std::string key,
                           std::string value) {
    L_(info) << "Publishing " << value << " to " << make_address("", "")
    << prefix;
    CURL *hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, make_address(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str()));
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);
    
    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) L_(error) << curl_easy_strerror(ret) << std::endl;
    curl_easy_cleanup(hnd);
}

std::pair <enum Flags, int> EtcdClient::get_req(std::string prefix, std::string key) {
    std::string data;
    CURL *hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, make_address(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&data));
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, &write_callback);
    
    CURLcode ret = curl_easy_perform(hnd);

    if (ret != CURLE_OK){
        L_(error) << "Get Request from key-value store error: ";
        L_(error) << curl_easy_strerror(ret) << std::endl;
    }

    std::pair <enum Flags, int> returnvalue = parse_value(data);
    curl_easy_cleanup(hnd);

    return returnvalue;
}

std::pair <enum Flags, int> EtcdClient::parse_value(std::string data) {
    Json::Value message;
    Json::Reader reader;
    std::pair <enum Flags, int> returnvalue;
    returnvalue = std::make_pair(errorneous, 1);
    

    bool parsingSuccessful = reader.parse(data, message);
    if (!parsingSuccessful)
        L_(warning) << "EtcdClient: Failed to parse";
    if (message.size() == 0) {
            L_(warning) << "EtcdClient: returned value from key-value store "
            "was empty, waiting for updates";
            returnvalue.first = ok;
            return returnvalue;
    }

    if (message.isMember("errorCode")){
        L_(error) << message["message"].asString() << std::endl;
    }
    else
         returnvalue = check_value(message);
    
    return returnvalue;
}

std::pair <enum Flags, int> EtcdClient::check_value(Json::Value message) {
    Flags check_flag = errorneous;
    std::pair <enum Flags, int> returnvalue;
    int requiredtag = 1;
    std::string value = message["node"]["value"].asString();
    std::string tag = message["node"]["modifiedIndex"].asString();

    if (value == "on") {
        check_flag = ok;
    } else {
        requiredtag = stoi(tag) + 1;
        check_flag = notupdated;
    }

    return returnvalue = std::make_pair(check_flag, requiredtag);
}

int EtcdClient::check_process(std::string input_shm) {
    std::stringstream prefix;
    int ret;
    prefix << "/" << input_shm;
    ret = get_req(prefix.str(), "/uptodate").first;
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

std::string EtcdClient::make_address(std::string prefix, std::string key) {
    std::ostringstream address;
    address << m_url << prefix << key;

    return address.str();
}

int EtcdClient::wait_value(std::string prefix) {
    Flags wait_flag = empty;
    int requiredtag = 1;
    
    while ((wait_flag == empty) || (wait_flag == notupdated)) {
        std::string data = wait_req(prefix, "/uptodate?wait=true&waitIndex=" + std::to_string(requiredtag));
        std::pair<enum Flags, int> returnvalue = parse_value(data);
        wait_flag = returnvalue.first;
        requiredtag = returnvalue.second;
    }
    return wait_flag;
}
