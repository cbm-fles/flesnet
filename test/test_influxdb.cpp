#include "influxdb.hpp"
#include <iostream>

//###################################################################

int main() {
  influxdb::db db("localhost", 8086, "test_db", "admin", "admin");
  influxdb::measurement m;

  std::string tag1_name = "high";
  bool bool1 = true;
  double doub1 = 1.24;
  int64_t int1 = 42;

  m.addTag("tag1_name", tag1_name);
  m.addField("bool1", bool1);
  m.addField("doub1", doub1);
  m.addField("int1", int1);

  try {
    db.sendSync(m, "measurement_test", db.defaultTimestamp());
  } catch (web::http::http_exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
