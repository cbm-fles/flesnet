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
        cout << "ret was " << ret << " (1 shm not uptodate, 2 an error occured)" << endl;
        //L_(warning) << "no shm set yet";
        cout << "no shm set yet" << endl;
        ret = waitvalue(prefix.str());
        if(ret != 0){
            cout << "ret was " << ret << " (1 shm not uptodate, 2 an error occured)" << endl;
            //L_(warning) << "no shm set";
            cout << "no shm set" << endl;
            exit (EXIT_FAILURE);
        }
    }
    //setvalue(prefix.str(),"uptodate", "value=off");
    http.putreq(prefix.str(),"uptodate","value=off", "PUT");
    //L_(info) << "flag for shm was set in kv-store";
    cout << "flag for shm was set in kv-store" << endl;
    
    return ret;
}


void EtcdClient::setvalue(string prefix, string key, string value){
    http.putreq(prefix,key,value, "PUT");
    /*cout << "posting " << value <<  " (" << strlen(value.c_str()) << ") to " << setadress(prefix, key) << endl;
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix, key).c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str()));
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

    ret = curl_easy_perform(hnd);
    if( ret != 0){
        cout << curl_easy_strerror(ret) << endl;
    }*/
    
}



string EtcdClient::setadress(string prefix, string key){
    ostringstream adress;
    adress << url << prefix << key;
    
    return adress.str();
}


int EtcdClient::getvalue(string prefix, string key){
    Json::Value message;
    Json::Reader reader;
    Json::FastWriter fastwriter;

    int flag = 2;
    

    
    cout << setadress(prefix,key) << endl;
    string os = http.getreq(prefix, key);
    
    
    bool parsingSuccessful = reader.parse(os, message);
    if (!parsingSuccessful)cout << "EtcdClient: Failed to parse" << endl;
    
    if(message.isMember("error")){
        flag = 2;
        cout << value << " " << fastwriter.write(message["error"]) << endl;
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
    cout << "uptodate is " << value << " with tag " << tag << endl;
    
    if (value == "on"){
        cout << value << endl;
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
    int flag = 2;
    int taglength = strlen(to_string(requiredtag).c_str());

    ostringstream key_ss;
    key_ss << "/uptodate?wait=true&waitIndex=" << requiredtag;
    string key = key_ss.str();
    
    //key ends with ?wait=true
    /*curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix,key).c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    
    cout << setadress(prefix,key) << endl;
    ret = curl_easy_perform(hnd);*/
    int ret = http.waitreq(prefix,key);
    if( ret == 0){
        //cut ?wait=true from key to get value without waiting
        key.erase(key.end()-(21+taglength),key.end());
        flag = getvalue(prefix, key);
    }
    
    return flag;
}
