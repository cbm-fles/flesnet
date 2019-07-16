/**
 * @file
 * @author Maximilian Kordt <maximilian.kordt@web.de>
 */

#include <iostream>
#include <cpprest/http_client.h>
#include <string>
#include <cpprest/json.h>


void something_went_wrong(web::http::http_response http_response, int task) {

  if (task == 0) { std::cout << "At task 'check_db_exists'." << std::endl; }
  if (task == 1) { std::cout << "At task 'create_database'." << std::endl; }

  std::cout << "Something went wrong. Server returned status code: " << http_response.status_code() << std::endl;
  std::cout << "For further information on status codes please visit ";
  std::cout << "'https://docs.influxdata.com/influxdb/v1.7/tools/api/'";
  std::cout << std::endl;

  pplx::task<void> jsonTask = http_response.extract_json().then([&] (web::json::value json_value) {

    std::cout << json_value << std::endl;

  }); 

  try {

    jsonTask.wait();

  }
  catch (const std::exception& e) {

    std::cout << "web::http::http_exception: " << e.what() << std::endl;

  }

  throw std::exception();

}

pplx::task<bool> check_db_exists (std::string db_name_, web::http::client::http_client http_client) {

  std::string db_name = db_name_;
  web::uri_builder uri_builder(U("/query"));
  uri_builder.append_query(U("q=SHOW+DATABASES"));

  std::string db_name_new = "[\"";
  db_name_new.append(db_name);
  db_name_new.append("\"]");

  return http_client.request(web::http::methods::GET, uri_builder.to_string())

  .then([=] (web::http::http_response http_response) {

    if (http_response.status_code() != 200) {

      something_went_wrong(http_response, 0);

    }

    return http_response.extract_json();

  })

  .then([=] (web::json::value json_value) {

    std::string json_string = json_value.serialize();
    size_t found_begin = json_string.find("\"values\":[") + 10;
    size_t found_end = json_string.find("]}],\"statement_id\"");
    //values as string [“db0"],["db1"],["db2"],....
    std::string values_string = json_string.substr(found_begin,found_end);
    //parsing the string containing all databases
    size_t pos = 0;
    std::string delimiter = ",";
    std::string db_string;

    while ((pos = values_string.find("statement_id")) != std::string::npos) {

        values_string = values_string.substr(0, pos);

    }

    while ((pos = values_string.find(delimiter)) != std::string::npos) { 

      db_string = values_string.substr(0, pos);

      values_string.erase(0, pos + delimiter.length());
      if (db_string.compare(db_name_new) == 0) {

        //*db_p = true;
        std::cout << "Databse does exists." << std::endl;
        return true;
        
      }
    }

    pos = values_string.find("]");
    db_string = values_string.substr(0, (pos + 1));

    if (db_string.compare(db_name_new) == 0) {

      //*db_p = true;
      std::cout << "Databse does exists." << std::endl;
      return true;
      
    }
    else {

      return false;

    }
  });
}

pplx::task<void> create_database (std::string db_name_, web::http::client::http_client http_client) {

  std::string db_name = db_name_;
  web::http::http_response http_response;

  std::string build = "q=CREATE+DATABASE+";
  build.append(db_name_);
      
  web::uri_builder uri_builder(U("/query"));
  uri_builder.append_query(U(build));

  return http_client.request(web::http::methods::POST, uri_builder.to_string())

  .then([&] (web::http::http_response http_response) {

    if (http_response.status_code() != 200) {

      something_went_wrong(http_response, 1);

    }
    else {

      std::cout << "Database created successfully." << std::endl;

    }


  });
}

int main () {

  std::string url = "";
  std::string db = "";
  std::string user = "admin";
  std::string password = "admin";
  std::string answer = "";
  std::string yes_ans = "yes";

  try {
    std::cout << "This is the initial setup for flib_status_db." << std::endl;
    std::cout << "Please enter the host and the designated database." << std::endl;
    std::cout << "DB host: " << std::endl;
    std::getline(std::cin, url);
    std::cout << "DB to use: " << std::endl;
    std::getline(std::cin, db);

    std::cout << "InfluxDB adress set as: " << url << std::endl;
    std::cout << "Target database set as: " << db << std::endl;

  /*
    For creation of the database admin rights required.
    Just in case user credentials are to be checked (Set user, password to empty).

    std::cout << "db_username: " << std::endl;
    std::getline(std::cin, user);
    std::cout << "db_user_password: " << std::endl;
    std::getline(std::cin, password);
  */
    //set credentials for http client
    web::http::client::http_client_config http_client_config;
    web::credentials credentials(U(user), U(password));
    http_client_config.set_credentials(credentials);
    //http client 
    web::http::client::http_client http_client(url, http_client_config);

    if (!check_db_exists(db, http_client).get()) {

      std::cout << "Designated database does not exist." << std::endl;
      std::cout << "Proceed to create it(" << db << ")?" << std::endl;
      std::cout << "yes/no: " << std::endl;
      std::getline(std::cin, answer);

      if (yes_ans.compare(answer) == 0) {

        create_database(db, http_client);

      }
      else {

        std::cout << "Exiting..." << std::endl;
        return EXIT_SUCCESS;

      }

    }
    else {

      std::cout << "Setup complete." << std::endl;
      return EXIT_SUCCESS;

    }
  }
  catch (const std::exception& e) {

    std::cout << "Exception: " << e.what() << std::endl;

  }

  return EXIT_SUCCESS;
}