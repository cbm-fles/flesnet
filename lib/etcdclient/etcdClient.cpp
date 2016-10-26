// Copyright 2016 Helvi Hartmann

#include "etcdClient.h"

EtcdClient::EtcdClient(string url_):
    url(url_),
    http(url)
{

  //hnd = curl_easy_init();
}


int EtcdClient::checkonprocess(string input_shm){
    stringstream prefix;
    int ret;
    prefix << "/" << input_shm;
    ret = getvalue(prefix.str(), "/uptodate");
    if(ret != 0) {
        L_(warning) << "no shm set in key-value store...waiting";
        ret = waitvalue(prefix.str());
        if(ret != 0){
            L_(error) << "return flag was " << ret << ". Exiting";
            exit (EXIT_FAILURE);
        }
    }
    
    return ret;
}

string EtcdClient::setadress(string prefix, string key){
    ostringstream adress;
    adress << url << prefix << key;
    
    return adress.str();
}

int EtcdClient::getvalue(string prefix, string key){
    int flag = parsevalue(http.getreq(prefix, key));
    return flag;
}

int EtcdClient::parsevalue(string data) {
    Json::Value message;
    Json::Reader reader;
    Json::FastWriter fastwriter;
    int flag = 2;

    bool parsingSuccessful = reader.parse(data, message);
    if (!parsingSuccessful) {
        L_(warning) << "EtcdClient: Failed to parse";
        if (message.size() == 0) {
            L_(warning) << "EtcdClient: returned value from key-value store "
                           "was empty, waiting for updates";
            return flag = 3;
        }
    }

    // cout << data << " retrieved message is " << message.size() << endl;

    if(message.isMember("error")){
        flag = 2;
        L_(error) << value << " " << fastwriter.write(message["error"]) << endl;
    }
    else flag = checkvalue(message);
    
    
    return flag;
}

int EtcdClient::checkvalue(Json::Value message){
    Json::FastWriter fastwriter;
    int flag = 1;
    
    value = fastwriter.write(message["node"]["value"]);
    string tag = fastwriter.write(message["node"]["modifiedIndex"]);
    value.erase(value.end()-2,value.end());
    value.erase(0,1);
    // cout << "uptodate is " << value << " with tag " << tag << endl;

    if (value == "on"){
        flag = 0;
    }
    else{
        requiredtag = stoi(tag) + 1;
        flag = 1;
    }
    
    return flag;
}


int EtcdClient::waitvalue(string prefix){
    string answer;
    ostringstream adress;
    int flag = 3;
    string data;
    ostringstream key_ss;
    key_ss << "/uptodate?wait=true&waitIndex=" << requiredtag;
    string key = key_ss.str();

    while (flag == 3) {
        data = http.waitreq(prefix, key);
        flag = parsevalue(data);
    }
    L_(info) << "retrieved: " << data << " and flag " << flag;
    return flag;
}
