/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Maximilian Kordt <maximilian.kordt@web.de>
 */

#include "device_operator.hpp"
#include "flib.h"
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <cpprest/http_client.h>
#include <string>
#include <cpprest/json.h>
#include <boost/asio/ip/host_name.hpp>

// measurement interval (equals output interval)
constexpr uint32_t interval_ms = 1000;
constexpr bool clear_screen = true;
constexpr bool detailed_stats = true;

struct pci_perf_data_t {
  uint64_t cycle_cnt;
  uint64_t pci_stall;
  uint64_t pci_trans;
};

int s_interrupted = 0;
static void s_signal_handler(int signal_value) {
  (void)signal_value;
  s_interrupted = 1;
}

static void s_catch_signals(void) {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGABRT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);
}

//Anzahl der Durchläufe, bis die HTTP-Anfrage gestellt wird. 
//zusammen mit interval_ms wird die Häufigkeit und Größe der Anfragen gesteuert. 
//interval = 1000 (1s) <-- jede Sekunde Statusinfo ausgelesen
//batching_count = 1 <-- jede Sekunde eine Anfrage
//batching_count = 10 <-- 10 Sekunden Statusinfo gesammelt und dann eine Anfrage
int baching_count = 10;


//extrahiere die fehlernachricht aus der antwort (json-objekt)
void something_went_wrong(web::http::http_response http_response, bool throw_true) {

  pplx::task<void> jsonTask = http_response.extract_json().then([&] (web::json::value json_value) {

    long int timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>\
    (std::chrono::system_clock::now().time_since_epoch()).count();
    std::stringstream ss;
    ss << json_value;
    std::cout << "timestamp: " << timestamp << " | Error: " << ss.str() << std::endl;

  }); 

  try {

    jsonTask.wait();

  }
  catch (const std::exception& e) {

    long int timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>\
    (std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "timestamp: " << timestamp << " | web::http::http_exception: " << e.what() << std::endl;

  }

  if (throw_true) { throw std::exception(); }

}

//senden einer http_request. "schreibe" in "db" dbname, typ der anfrage POST
//je nach write / get / etc. anpassen. influxDB docu "https://docs.influxdata.com/influxdb/v1.7/"
//set_body datensätze an request "anheften"
//
//pplx::task<return_type>
//legt fest, was am ende der taskkette zurückgegeben werden muss
pplx::task<void> http_post (std::string db, std::string db_input_string, web::http::client::http_client http_client) {

  web::uri_builder uri_builder(U("/write"));
  uri_builder.append_query(U("db"), U(db));
  web::http::http_request http_request; 
  http_request.set_body(db_input_string);

  //return was von http_client.request() zurückkommt. hier "http_response"
  return http_client.request(web::http::methods::POST, uri_builder.to_string(), http_request.body())

  //nächster Task nimmt diese response als eingabe und prüft den status-code
  .then([&] (web::http::http_response http_response) {

    if (http_response.status_code() != 204) {

      bool cancel = false;

      something_went_wrong(http_response, cancel);

    }
  });
}

//status der DB abfragen. aus der antwort wird ersichtlich, ob die DB verfügbar ist. 
pplx::task <void> ping_test (web::http::client::http_client http_client) {

  web::uri_builder uri_builder(U("/ping"));
  web::http::http_request http_request;

  return http_client.request(web::http::methods::GET, uri_builder.to_string())

  .then([&] (web::http::http_response http_response) {

    if (http_response.status_code() != 204) {

      bool cancel = true;
      something_went_wrong(http_response, cancel);

    }
  });
}

int main(int argc, char* argv[]) {
  s_catch_signals();

  //Vectoren um die Datensätze aus den beiden Schleifen zu bauen.
  std::vector <std::string> db_input_vector;
  std::vector <std::string> influx_base_tag_vector;
  std::vector <std::string> influx_base_field_vector;
  //URL der Datenbank, Datenbankname, Nutzername, Nutzer-Passwort, Host-Name
  //cbm01 hat noch die Standardkonfiguration für den Admin
  //Nutzer admin, Passwort admin
  //beim fles hat jeder Adminrechte und kann mit 
  // $ influx
  //auf die Datenbank zugreifen.
  //cbm01 ist zugriffsgeschützt.
  // $ influx --username admin --password admin
  //um sich mit Adminrechten einzuloggen.
  //
  //Die Form der url (http://...:port/ sollte eingehalten werden. 
  //Ansonsten funktioniert der http_client nicht.
  std::string url = "http://localhost:8086/";
  std::string db = "flib_data";
  std::string username = "admin";
  std::string password = "admin";
  auto host_name = boost::asio::ip::host_name();

  //http_client
  web::http::client::http_client_config http_client_config;
  web::credentials credentials(U(username), U(password));
  http_client_config.set_credentials(credentials);
  web::http::client::http_client http_client(url, http_client_config);

  try {

    ping_test(http_client);

    if (argc != 1) {

      (void)argv;

	    std::cout
	    << "Displays status and performance counters for all FLIB links.\n"
	       "Per FLIB counters:\n"
	       "idle:     PCIe interface is idle (ratio)\n"
	       "stall:    back pressure on PCIe interface from host (ratio)\n"
	       "trans:    data is transmitted via PCIe interface (ratio)\n"
	       "Per link status/counters:\n"
	       "link:     flib/link\n"
	       "data_sel: choosen data source\n"
	       "up:       flim channel_up\n"
	       "he:       aurora hard_error\n"
	       "se:       aurora soft_error\n"
	       "eo:       eoe fifo overflow\n"
	       "do:       data fifo overflow\n"
	       "d_max:    maximum number of words in d_fifo\n"
	       "dma_s:    stall from dma mux (ratio)\n"
	       "data_s:   stall from full data buffer (ratio)\n"
	       "desc_s:   stall from full desc buffer (ratio)\n"
	       "bp:       back pressure to link (ratio)\n"
	       "rate:     ms processing rate (Hz*)\n"
	       "* Based on the assumption that the PCIe clock is exactly 100 "
	       "MHz.\n"
	       "  This may not be true in case of PCIe spread-spectrum "
	       "clocking.\n";
	    std::cout << std::endl;

      return EXIT_SUCCESS;
    }

    std::unique_ptr<pda::device_operator> dev_op(new pda::device_operator);
    std::vector<std::unique_ptr<flib::flib_device_flesin>> flibs;
    uint64_t num_dev = dev_op->device_count();
    std::vector<flib::dma_perf_data_t> dma_perf_acc(num_dev);
    std::vector<pci_perf_data_t> pci_perf_acc(num_dev);
    std::vector<std::vector<flib::flib_link_flesin::link_perf_t>> link_perf_acc(
        num_dev);

    for (size_t i = 0; i < num_dev; ++i) {
      flibs.push_back(std::unique_ptr<flib::flib_device_flesin>(
          new flib::flib_device_flesin(i)));
      link_perf_acc.at(i).resize(flibs.at(i)->number_of_hw_links());
    }

    // set measurement interval for device and all links
    for (auto& flib : flibs) {
      flib->set_perf_interval(interval_ms);
      flib->get_dma_perf(); // dummy read to reset counters
      for (auto& link : flib->links()) {
        link->set_perf_interval(interval_ms);
      }
    }
    uint32_t pci_cycle_cnt = flibs.at(0)->get_perf_interval_cycles();

    int counter_i = 0;
    // main output loop
    size_t loop_cnt = 0;
    while (s_interrupted == 0) {

      size_t j = 0;
      for (auto& flib : flibs) {
        uint32_t pci_stall_cyl = flib->get_pci_stall();
        uint32_t pci_trans_cyl = flib->get_pci_trans();
        
        float pci_stall = pci_stall_cyl / static_cast<float>(pci_cycle_cnt);
        float pci_trans = pci_trans_cyl / static_cast<float>(pci_cycle_cnt);
        float pci_idle = 1 - pci_trans - pci_stall;

        std::stringstream input;

        std::string dev_info = flib->print_devinfo();
        //clear input
        input.str("");
        input << "flib_data," << "host=" << host_name << ",flib=" << dev_info;
        influx_base_tag_vector.push_back(input.str());
        //clear input
        input.str("");
        input << " pci_idle=" << pci_idle;
        input << ",pci_stall=" << pci_stall;
        input << ",pci_trans=" << pci_trans;

        if (detailed_stats) {
          flib::dma_perf_data_t dma_perf = flib->get_dma_perf();

      	  for (size_t i = 0; i <= 7; ++i) {
            input << ",fill" << i << "=" << dma_perf.fifo_fill[i];
          }

          influx_base_field_vector.push_back(input.str());

        }

        ++j;
      }
  
      j = 0;
      for (auto& flib : flibs) {
        long int ns = std::chrono::duration_cast<std::chrono::nanoseconds>\
        (std::chrono::system_clock::now().time_since_epoch()).count();
        int active_links = 0;
        size_t num_links = flib->number_of_hw_links();
        std::vector<flib::flib_link_flesin*> links = flib->links();

        std::stringstream ss;
        std::vector <std::string> influx_tag_vector;
        std::vector <std::string> influx_field_vector;
        for (size_t i = 0; i < num_links; ++i) {
          flib::flib_link_flesin::link_status_t status =
              links.at(i)->link_status();
          flib::flib_link_flesin::link_perf_t perf = links.at(i)->link_perf();

          float dma_stall =
              perf.dma_stall / static_cast<float>(perf.pkt_cycle_cnt) * 100.0;
          float data_buf_stall = perf.data_buf_stall /
                                 static_cast<float>(perf.pkt_cycle_cnt) * 100.0;
          float desc_buf_stall = perf.desc_buf_stall /
                                 static_cast<float>(perf.pkt_cycle_cnt) * 100.0;
          float din_full = perf.din_full_gtx /
                           static_cast<float>(perf.gtx_cycle_cnt) * 100.0;
          float event_rate =
              perf.events /
              (static_cast<float>(perf.pkt_cycle_cnt) / flib::pkt_clk);

          std::stringstream data_sel_stream;
          data_sel_stream << links.at(i)->data_sel();
          std::string data_sel_string = data_sel_stream.str();    // <-- Hier nach

          //kommt "   pgen" zurück. mit drei leerzeichen am Anfang. "disable" funktioniert ohne Probleme.
          //Damit konnte die Datenbank nicht richtig umgehen und hat immer falsche syntax zurückgegeben.
          //Deswegen habe ich die ersten 3 Zeichen des strings entfernt, wenn er nicht "disable" ist.
          //Die anderen Arten müssten wahrscheinlich ebenfalls exludiert werden.
          if (data_sel_string.compare("disable") != 0) {
            ++active_links;

            data_sel_string = data_sel_string.substr(3, data_sel_string.length());

          }

          std::stringstream input;
          //clear input
          input.str("");
          input << ",link=" << i;
          input << ",data_sel=" << "\"" << data_sel_string << "\"";

          std::string temp = influx_base_tag_vector.at(j);
          influx_tag_vector.push_back(temp.append(input.str()));
          //clear input
          input.str("");
          //fields
          if (status.channel_up) {

          	input << ",up=" << "TRUE";

          }
          else {

          	input << ",up=" << "FALSE";

          }

          input << ",d_max=" << status.d_fifo_max_words << "i";
          input << ",dma_s=" << dma_stall;
          input << ",desc_s=" << desc_buf_stall;
          input << ",data_s=" << data_buf_stall;
          input << ",din_full=" << din_full;
          input << ",event_rate=" << event_rate;

          if (status.soft_err) { input << ",se=" << "TRUE"; }
          else { input << ",se=" << "FALSE"; }
          if (status.hard_err) { input << ",he=" << "TRUE"; }
          else { input << ",he=" << "FALSE"; }
          if (status.eoe_fifo_overflow) { input << ",eo=" << "TRUE"; }
          else { input << ",eo=" << "FALSE"; }
          if (status.d_fifo_overflow) { input << ",do=" << "TRUE"; }
          else { input << ",do=" << "FALSE"; }

          input << ",active_links=" << active_links << "i";
          //timestamp
          input << " " << ns;

          //Syntax
          //<measurement>,<tag>,...,<tag> <field>,...,<field> <timestamp>
          std::string temp2 = influx_base_field_vector.at(j);
          std::string fields = temp2.append(input.str());
          std::string temp3 = influx_tag_vector.at(i);
          db_input_vector.push_back(temp3.append(fields)); 

        }

        ++j;

      }

      influx_base_field_vector.clear();

      //building string to post via the http request
      std::stringstream db_input;

      for (unsigned int i = 0; i < db_input_vector.size(); ++i) {

        db_input << db_input_vector.at(i) << "\n";

      }

      std::string db_input_string = db_input.str();
      db_input_vector.clear();

      try {
        //Wenn die Anzahl Durchläufe die festgelegt batchinggröße
        //erreicht hat, wird die http_request gesendet. 
        if (counter_i == baching_count) {

          http_post(db, db_input_string, http_client);
          counter_i = 0;

        }

      }
      catch (const std::exception& e) {

        std::cout << e.what() << std::endl;
        return EXIT_SUCCESS;

      }

      ++counter_i;

      // sleep will be canceled by signals (which is handy in our case)
      // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66803
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
      ++loop_cnt;
    }
  }
  catch (const std::exception& e) {

    std::cout << "std::exception: " << e.what() << std::endl;

    return EXIT_FAILURE;

  }

  return EXIT_SUCCESS;
}