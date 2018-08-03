// Copyright 2017-2018, Julian Reinhardt <jreinhardt@compeng.uni-frankfurt.de>

#pragma once
#include <cpprest/http_client.h>
#include <sstream>
#include <string>

namespace influxdb {
class measurement {
  std::unordered_map<std::string, std::string> tagMap;
  std::unordered_map<std::string, std::string> fieldMap;

public:
  template <class T> void addTag(std::string const& key, T const& value) {
    tagMap.insert(measurementPar(key, value));
  }

  template <class T> void addField(std::string const& key, T const& value) {
    fieldMap.insert(measurementPar(key, value));
  }

  std::unordered_map<std::string, std::string> getTagMap() const;
  std::unordered_map<std::string, std::string> getFieldMap() const;

  std::pair<std::string, std::string> measurementPar(std::string k,
                                                     uint64_t v) {
    return std::make_pair<>(k, std::to_string(v) + "i");
  };
  std::pair<std::string, std::string> measurementPar(std::string k, int64_t v) {
    return std::make_pair<>(k, std::to_string(v) + "i");
  };
  std::pair<std::string, std::string> measurementPar(std::string k, double v) {
    return std::make_pair<>(k, std::to_string(v));
  };
  std::pair<std::string, std::string> measurementPar(std::string k, bool v) {
    return std::make_pair<>(k, (v ? "true" : "false"));
  };
  std::pair<std::string, std::string> measurementPar(std::string k,
                                                     std::string v) {
    return std::make_pair<>(k, ("\"" + v + "\""));
  };
};

class db {
  web::http::client::http_client client;
  web::uri uriDB;
  std::string username;
  std::string password;

public:
  pplx::task<void> requestTask;
  std::string post(std::string const& query);
  db(std::string const& address_,
     uint32_t port_,
     std::string const& databaseName_,
     std::string const& username_,
     std::string const& password_);
  std::string urlBuilder(std::string const& name_, uint32_t port_);
  std::size_t defaultTimestamp();
  std::string
  requestBuilder(std::string const& measurementName,
                 std::unordered_map<std::string, std::string> const& tagMap,
                 std::unordered_map<std::string, std::string> const& fieldMap,
                 size_t const& timestamp);
  std::string
  getMapString(std::unordered_map<std::string, std::string> const& dataMap);
  pplx::task<void> sendAsync(measurement const& m,
                             std::string const& measurementName,
                             size_t const& timestamp);
  void sendSync(measurement const& m,
                std::string const& measurementName,
                size_t const& timestamp);
};
}
