// Copyright 2017-2018, Julian Reinhardt <jreinhardt@compeng.uni-frankfurt.de>

#include "influxdb.hpp"
#include "log.hpp"
#include <chrono>
#include <cpprest/http_client.h>
#include <cpprest/streams.h>

namespace {
inline void throwResponse(web::http::http_response const& response) {
  throw std::runtime_error(response.extract_string().get());
}

inline web::http::http_request
requestFrom(web::http::uri const& uriDB,
            std::string const& lines,
            std::string const& username,
            std::string const& password,
            web::http::method m = web::http::methods::POST) {
  web::http::http_request request;

  request.set_request_uri(uriDB);
  request.set_method(m);

  if (!username.empty()) {
    auto auth = username + ":" + password;
    std::vector<unsigned char> bytes(auth.begin(), auth.end());
    request.headers().add(web::http::header_names::authorization,
                          ("Basic ") + utility::conversions::to_base64(bytes));
  }

  request.set_body(lines);
  return request;
}
}

influxdb::db::db(std::string const& address_,
                 uint32_t port_,
                 std::string const& databaseName_,
                 std::string const& username_,
                 std::string const& password_)
    : client(urlBuilder(address_, port_)), username(username_),
      password(password_) {
  web::http::uri_builder builder(client.base_uri());
  builder.append("/write");
  builder.append_query(("db"), databaseName_);
  uriDB = builder.to_uri();
}

std::string influxdb::db::urlBuilder(std::string const& name_, uint32_t port_) {
  std::stringstream url;
  url << "http://" << name_ << ":" << port_;
  return url.str();
}

std::string influxdb::db::post(std::string const& query) {
  web::http::uri_builder builder(("/query"));
  builder.append_query(("q"), query);

  auto response =
      client.request(requestFrom(builder.to_string(), "", username, password))
          .get();
  if (response.status_code() == web::http::status_codes::OK) {
    return response.extract_string().get();
  } else {
    throwResponse(response);
    return std::string();
  }
}

std::string influxdb::db::requestBuilder(
    std::string const& measurementName,
    std::unordered_map<std::string, std::string> const& tagMap,
    std::unordered_map<std::string, std::string> const& fieldMap,
    size_t const& timestamp) {
  std::stringstream requestString;
  requestString << measurementName << "," << getMapString(tagMap)
                << getMapString(fieldMap) << " " << timestamp;
  L_(trace) << requestString.str();
  return requestString.str();
}

std::string influxdb::db::getMapString(
    std::unordered_map<std::string, std::string> const& dataMap) {
  std::stringstream mapString;
  for (auto it = dataMap.begin(); it != dataMap.end(); ++it) {
    if ((it != dataMap.end()) && (next(it) == dataMap.end())) {
      mapString << it->first << "=" << it->second << " ";
    } else {
      mapString << it->first << "=" << it->second << ",";
    }
  }
  return mapString.str();
}

void influxdb::db::sendSync(measurement const& m,
                            std::string const& measurementName,
                            size_t const& timestamp) {
  sendAsync(m, measurementName, timestamp).get();
}

pplx::task<void> influxdb::db::sendAsync(measurement const& m,
                                         std::string const& measurementName,
                                         size_t const& timestamp) {
  std::string requestString = requestBuilder(measurementName, m.getTagMap(),
                                             m.getFieldMap(), timestamp);

  requestTask =
      client.request(requestFrom(uriDB, requestString, username, password))
          .then([=](web::http::http_response response) {
            L_(trace) << "Received response status code:"
                      << response.status_code();
            if (!(response.status_code() == web::http::status_codes::OK ||
                  response.status_code() ==
                      web::http::status_codes::NoContent)) {
              throwResponse(response);
            }
          });
  return requestTask;
}

size_t influxdb::db::defaultTimestamp() {
  size_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
  return now;
}

std::unordered_map<std::string, std::string>
influxdb::measurement::getTagMap() const {
  return tagMap;
}
std::unordered_map<std::string, std::string>
influxdb::measurement::getFieldMap() const {
  return fieldMap;
}
