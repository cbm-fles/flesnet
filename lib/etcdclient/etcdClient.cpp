#include "etcdClient.h"

EtcdClient::EtcdClient(string url_):
    url(url_)
{

  hnd = curl_easy_init();
    cout << url << endl;
  curl_easy_setopt(hnd, CURLOPT_URL, url.c_str());
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
}

void EtcdClient::setvalue(string value){
    
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, value.c_str());
    curl_easy_setopt(hnd, CURLOPT_POSTFIELDSIZE_LARGE, strlen(value.c_str())-1);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

    ret = curl_easy_perform(hnd);
    if( ret != 0){
        cout << curl_easy_strerror(ret) << endl;
    }
    
}

string EtcdClient::setadress(string key){
    ostringstream adress;
    adress << url << key;
    
    return adress.str();
}

string EtcdClient::getvalue(string key){
    Json::Value message;
    Json::Reader reader;
    Json::FastWriter fastwriter;
    ostringstream os;
    
    cout << setadress(key) << endl;
    os << curlpp::options::Url(setadress(key));

    bool parsingSuccessful = reader.parse(os.str(), message);
    if (!parsingSuccessful)cout << "Failed to parse" << endl;
    //cout << "key is " << message["node"] << endl;
    
    string answer = fastwriter.write(message["node"]["value"]).erase(0,1);
    answer.erase(answer.end()-2, answer.end());
    
    return answer;
}


string EtcdClient::waitvalue(string key){
    curl_easy_setopt(hnd, CURLOPT_URL, setadress(key).c_str());
    //curl_easy_setopt(hnd, CURLOPT_URL, "http://10.0.100.10:2379/v2/keys/shm?wait=true");
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.35.0");
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    
    return getvalue(key);
}


EtcdClient::EtcdClient(){
  curl_easy_cleanup(hnd);
  hnd = NULL;
  cout << "curl handle destroyed" << endl;

}
