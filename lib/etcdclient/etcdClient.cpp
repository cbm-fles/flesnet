#include "etcdClient.h"

EtcdClient::EtcdClient(string url_):
    url(url_)
{

  hnd = curl_easy_init();
}

void EtcdClient::setvalue(string prefix, string key, string value){
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
    }
    
}

string EtcdClient::setadress(string prefix, string key){
    ostringstream adress;
    adress << url << prefix << key;
    
    return adress.str();
}


int EtcdClient::getvalue(string prefix, string key){
    Json::Value message;
    int flag = 2;
    ostringstream os;
    Json::Reader reader;
    Json::FastWriter fastwriter;

    cout << setadress(prefix,key) << endl;
    os << curlpp::options::Url(setadress(prefix,key));
    
    bool parsingSuccessful = reader.parse(os.str(), message);
    if (!parsingSuccessful)cout << "Failed to parse" << endl;
    // cout << "key is " << message << endl;
    
    value = fastwriter.write(message["node"]["value"]);
    value.erase(value.end()-2,value.end());
    value.erase(0,1);
    
    if(message.isMember("error")){
        flag = 2;
        cout << value << " " << fastwriter.write(message["error"]) << endl;
    }
    else flag = 0;
    
    return flag;
}

int EtcdClient::getuptodatevalue(string prefix, bool uptodate){
    ostringstream os;
    string key;
    int flag = 2;

    
    if(!uptodate)key = "/uptodate";
    else key = "/shmname";
    
    flag = getvalue(prefix,key);
    

    string answer;
    cout << value << endl;
    if(!uptodate){
        flag = 1;
        if(value == "yes"){
            cout << value << endl;
            flag = getuptodatevalue(prefix, true);
            setvalue(prefix, key, "value=no");
        }
    }
    else answer = value;

    return flag;
}


int EtcdClient::waitvalue(string prefix){
    string answer;
    ostringstream adress;
    int flag = 2;
    
    string key = "/uptodate?wait=true";

    //key ends with ?wait=true
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(prefix,key).c_str());
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 5000L);
    curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    
    ret = curl_easy_perform(hnd);
    if( ret != 0){
        cout << curl_easy_strerror(ret) << endl;
        flag = 2;
    }
    else{
        //cut ?wait=true from key to get value without waiting
        key.erase(key.end()-10,key.end());
        flag = getuptodatevalue(prefix, false);
    }
    
    return flag;
}


EtcdClient::EtcdClient(){
  curl_easy_cleanup(hnd);
  hnd = NULL;
  cout << "curl handle destroyed" << endl;

}
