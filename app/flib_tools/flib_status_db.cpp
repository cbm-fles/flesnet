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

void wrong_input() {

  std::cout << std::endl;
    std::cout << "Wrong parameter(s).";
    std::cout << std::endl;
    std::cout << "Correct parameters are 'help' or 'Terminal' or 'Grafana'";
    std::cout << std::endl;
    std::cout << "Type parameter 'help' only to get an overview of ";
    std::cout << "the shown of the Terminal view." << std::endl;
    std::cout << std::endl;

}

int went_wrong_counter = 0;

void something_went_wrong(web::http::http_response http_response, int task) {

  if (task == 0) { std::cout << "At task 'check_db_exists'."; }
  if (task == 1) { std::cout << "At task 'create_database'."; }
  if (task == 2) { std::cout << "At task 'http_post'."; }

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

    throw;

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

pplx::task<void> http_post (std::string db, std::string db_input_string, web::http::client::http_client http_client) {

  web::uri_builder uri_builder(U("/write"));
  uri_builder.append_query(U("db"), U(db));
  //actual http request
  web::http::http_request http_request; 
  http_request.set_body(db_input_string);

  return http_client.request(web::http::methods::POST, uri_builder.to_string(), http_request.body())

  .then([&] (web::http::http_response http_response) {

    if (http_response.status_code() != 204) {

      something_went_wrong(http_response, 2);

    }
    //else { std::cout << "Success!" << std::endl; }
  });
}

int main(int argc, char* argv[]) {
  s_catch_signals();

  bool terminal = false;
  bool grafana = false;
  std::vector <std::string> db_input_vector;
  std::vector <std::string> influx_base_tag_vector;
  std::vector <std::string> influx_base_field_vector;
  std::string help_str = "help";
  std::string terminal_str = "Terminal";
  std::string grafana_str = "Grafana";
  std::string url = "http://localhost:8086/"; //fast testing
  std::string db = "flib_data";
  std::string user = "admin";
  std::string password = "admin";
  std::vector<web::http::client::http_client> http_client_vector;
  auto host_name = boost::asio::ip::host_name();

  try {
    // display help if any parameter other the required is given
    if (argc == 2) {

    	//if (argv[1] == "help") {
      if (help_str.compare(argv[1]) == 0) {

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
      	//else if (argv[1] == "Terminal") {
      else if (terminal_str.compare(argv[1]) == 0) {

      	terminal = true;

      }
      //else if (argv[1] == "Grafana") {
      else if (grafana_str.compare(argv[1]) == 0) {

      	grafana = true;
/*schnelleres testen ohne usereingabe
        std::cout << "DB Location: " << std::endl;
        std::getline(std::cin, url);
        std::cout << "DB to use: " << std::endl;
        std::getline(std::cin, db);

        std::cout << "InfluxDB adress set as: " << url << std::endl;
        std::cout << "Target database set as: " << db << std::endl;

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
        http_client_vector.push_back(http_client);

        if (!check_db_exists(db, http_client).get()) {

          create_database(db, http_client);

        }

      }
      else {

      	wrong_input();

      	return EXIT_SUCCESS;

      }
    }
    else {

      wrong_input();

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

    if (terminal) {

      std::cout << "Starting measurements" << std::endl;
      if (clear_screen) {
        std::cout << "\x1B[2J" << std::flush;
      }

    }

    // main output loop
    size_t loop_cnt = 0;
    while (s_interrupted == 0) {

      if (terminal) {
        if (clear_screen) {
          std::cout << "\x1B[H" << std::flush;
        }
        std::cout << "Measurement " << loop_cnt << ":" << std::endl;
      }

      size_t j = 0;
      for (auto& flib : flibs) {
        uint32_t pci_stall_cyl = flib->get_pci_stall();
        uint32_t pci_trans_cyl = flib->get_pci_trans();

        if (terminal) {

          pci_perf_acc.at(j).cycle_cnt += pci_cycle_cnt;  //?
          pci_perf_acc.at(j).pci_stall += pci_stall_cyl;  //?
          pci_perf_acc.at(j).pci_trans += pci_trans_cyl;  //?

        }
        
        float pci_max_stall = flib->get_pci_max_stall();
        float pci_stall = pci_stall_cyl / static_cast<float>(pci_cycle_cnt);
        float pci_trans = pci_trans_cyl / static_cast<float>(pci_cycle_cnt);
        float pci_idle = 1 - pci_trans - pci_stall;

        if (terminal) {

          float pci_stall_acc = pci_perf_acc.at(j).pci_stall /
                              static_cast<float>(pci_perf_acc.at(j).cycle_cnt);
          float pci_trans_acc = pci_perf_acc.at(j).pci_trans /
                                static_cast<float>(pci_perf_acc.at(j).cycle_cnt);
          float pci_idle_acc = 1 - pci_trans_acc - pci_stall_acc;

          //status fenster oberste paar zeilen
          std::cout << "FLIB " << j << " (" << flib->print_devinfo() << ")"
                    << std::endl;
          std::cout << std::setprecision(4) << "PCIe idle " << std::setw(9)
                    << pci_idle << "   stall " << std::setw(9) << pci_stall
                    << " (max. " << std::setw(5) << pci_max_stall << " us)"
                    << "   trans " << std::setw(9) << pci_trans << std::endl;
          std::cout << std::setprecision(4) << "avg.      " << std::setw(9)
                    << pci_idle_acc << "         " << std::setw(9)
                    << pci_stall_acc << "                "
                    << "         " << std::setw(9) << pci_trans_acc << std::endl;
        }

        std::stringstream input;

        if (grafana) {

          //flib_data,flib=01:00:00,link=0 pci_idle=x1,...,dma_s=y1,...
          //flib_data,flib=01:00:00,link=1 pci_idle=x1,...,dma_s=y2,...
          std::string dev_info = flib->print_devinfo();
          //clear input
          input.str("");
          input << "flib_data," << "host=" << host_name << ",flib=" << dev_info;
          influx_base_tag_vector.push_back(input.str());
          //std::cout << "base tag-back: " << influx_base_tag_vector.back() << std::endl;
          //clear input
          input.str("");
          input << " pci_idle=" << pci_idle;
          input << ",pci_stall=" << pci_stall;
          input << ",pci_trans=" << pci_trans;

        }

        if (detailed_stats) {
          flib::dma_perf_data_t dma_perf = flib->get_dma_perf();

          if (terminal) {

            dma_perf_acc.at(j).overflow += dma_perf.overflow;
            dma_perf_acc.at(j).cycle_cnt += dma_perf.cycle_cnt;
            dma_perf_acc.at(j).fifo_fill[0] += dma_perf.fifo_fill[0];
            dma_perf_acc.at(j).fifo_fill[1] += dma_perf.fifo_fill[1];
            dma_perf_acc.at(j).fifo_fill[2] += dma_perf.fifo_fill[2];
            dma_perf_acc.at(j).fifo_fill[3] += dma_perf.fifo_fill[3];
            dma_perf_acc.at(j).fifo_fill[4] += dma_perf.fifo_fill[4];
            dma_perf_acc.at(j).fifo_fill[5] += dma_perf.fifo_fill[5];
            dma_perf_acc.at(j).fifo_fill[6] += dma_perf.fifo_fill[6];
            dma_perf_acc.at(j).fifo_fill[7] += dma_perf.fifo_fill[7];

            std::stringstream ss;
            ss << "fill     1/8     2/8     3/8     4/8     5/8     6/8     7/8  "
                  "   8/8    merr"
               << std::endl;
            ss << "    ";
            for (size_t i = 0; i <= 7; ++i) {
              ss << " " << std::setw(7) << std::fixed << std::setprecision(3)
                 << dma_perf.fifo_fill[i] / float(dma_perf.cycle_cnt) * 100.0;
            }
            ss << " " << std::setw(7) << dma_perf.overflow << std::endl;
            ss << "avg.";
            for (size_t i = 0; i <= 7; ++i) {
              ss << " " << std::setw(7) << std::fixed << std::setprecision(3)
                 << dma_perf_acc.at(j).fifo_fill[i] /
                        float(dma_perf_acc.at(j).cycle_cnt) * 100.0;
            }
            ss << " " << std::setw(7) << dma_perf_acc.at(j).overflow << std::endl;
            std::cout << ss.str();

          }
          if (grafana) {

        	  for (size_t i = 0; i <= 7; ++i) {
              /*input << ",fill" << i << "=" << std::setw(7) << std::setprecision(3)
                 << (dma_perf.fifo_fill[i] / float(dma_perf.cycle_cnt) * 100.0);*/
              //auto value = (dma_perf.fifo_fill[i] / float(dma_perf.cycle_cnt) * 100.0);
              input << ",fill" << i << "=" << dma_perf.fifo_fill[i];
            }

            influx_base_field_vector.push_back(input.str());
            //std::cout << "base field-back: " << influx_base_field_vector.back() << std::endl;

          }
        }

        ++j;
      }

      if (terminal) {

        std::cout << std::endl;

        std::cout << "link  data_sel  up  d_max        bp         ∅     "
                     "dma_s         ∅    data_s    "
                     "     ∅    desc_s         ∅     rate"
                     "         ∅  he  se  eo  do\n";

      }
  
      j = 0;
      for (auto& flib : flibs) {
        size_t num_links = flib->number_of_hw_links();
        std::vector<flib::flib_link_flesin*> links = flib->links();

        std::stringstream ss;
        std::vector <std::string> influx_tag_vector;
        std::vector <std::string> influx_field_vector;
        for (size_t i = 0; i < num_links; ++i) {
          flib::flib_link_flesin::link_status_t status =
              links.at(i)->link_status();
          flib::flib_link_flesin::link_perf_t perf = links.at(i)->link_perf();

          if (terminal) {

            link_perf_acc.at(j).at(i).pkt_cycle_cnt += perf.pkt_cycle_cnt;
            link_perf_acc.at(j).at(i).dma_stall += perf.dma_stall;
            link_perf_acc.at(j).at(i).data_buf_stall += perf.data_buf_stall;
            link_perf_acc.at(j).at(i).desc_buf_stall += perf.desc_buf_stall;
            link_perf_acc.at(j).at(i).events += perf.events;
            link_perf_acc.at(j).at(i).gtx_cycle_cnt += perf.gtx_cycle_cnt;
            link_perf_acc.at(j).at(i).din_full_gtx += perf.din_full_gtx;

          }

          float dma_stall =
              perf.dma_stall / static_cast<float>(perf.pkt_cycle_cnt) * 100.0;
          float data_buf_stall = perf.data_buf_stall /
                                 static_cast<float>(perf.pkt_cycle_cnt) * 100.0;
          float desc_buf_stall = perf.desc_buf_stall /
                                 static_cast<float>(perf.pkt_cycle_cnt) * 100.0;
          //float din_full = perf.din_full_gtx /
          //                 static_cast<float>(perf.gtx_cycle_cnt) * 100.0;
          float din_full = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/100));
          float event_rate =
              perf.events /
              (static_cast<float>(perf.pkt_cycle_cnt) / flib::pkt_clk);

          if (terminal) {

            float dma_stall_acc =
                link_perf_acc.at(j).at(i).dma_stall /
                static_cast<float>(link_perf_acc.at(j).at(i).pkt_cycle_cnt) *
                100.0;
            float data_buf_stall_acc =
                link_perf_acc.at(j).at(i).data_buf_stall /
                static_cast<float>(link_perf_acc.at(j).at(i).pkt_cycle_cnt) *
                100.0;
            float desc_buf_stall_acc =
                link_perf_acc.at(j).at(i).desc_buf_stall /
                static_cast<float>(link_perf_acc.at(j).at(i).pkt_cycle_cnt) *
                100.0;
            float din_full_acc =
                link_perf_acc.at(j).at(i).din_full_gtx /
                static_cast<float>(link_perf_acc.at(j).at(i).gtx_cycle_cnt) *
                100.0;
            float event_rate_acc =
                link_perf_acc.at(j).at(i).events /
                (static_cast<float>(link_perf_acc.at(j).at(i).pkt_cycle_cnt) /
                 flib::pkt_clk);

            ss << std::setw(2) << j << "/" << i << "  ";
            ss << std::setw(8) << links.at(i)->data_sel() << "  ";
            // status
            ss << std::setw(2) << status.channel_up << "  ";
            ss << std::setw(5) << status.d_fifo_max_words << "  ";
            // perf counters
            ss << std::setprecision(3); // percision + 5 = width
            ss << std::setw(8) << din_full << "  " << std::setw(8) << din_full_acc
               << "  ";
            ss << std::setw(8) << dma_stall << "  " << std::setw(8)
               << dma_stall_acc << "  ";
            ss << std::setw(8) << data_buf_stall << "  " << std::setw(8)
               << data_buf_stall_acc << "  ";
            ss << std::setw(8) << desc_buf_stall << "  " << std::setw(8)
               << desc_buf_stall_acc << "  ";
            ss << std::setprecision(7) << std::setw(7) << event_rate << "  "
               << std::setw(8) << event_rate_acc << "  ";
            // error
            ss << std::setw(2) << status.hard_err << "  ";
            ss << std::setw(2) << status.soft_err << "  ";
            ss << std::setw(2) << status.eoe_fifo_overflow << "  ";
            ss << std::setw(2) << status.d_fifo_overflow << "  ";

            ss << "\n";

          }
          if (grafana) {

            std::stringstream data_sel_stream;
            data_sel_stream << links.at(i)->data_sel();
            std::string data_sel_string = data_sel_stream.str();
            if (data_sel_string.compare("disable") != 0) {

              data_sel_string = data_sel_string.substr(3, data_sel_string.length());

            }

            std::stringstream input;
            //clear input
            input.str("");
            //std::cout << "input: " << input.str() << std::endl;
            input << ",link=" << i;
            input << ",data_sel=" << data_sel_string;
            //flib_data,flib=01:00:00,link=0 pci,stall,trans,fill
            //std::cout << "j: " << j << std::endl;
            std::string temp = influx_base_tag_vector.at(j);
            //std::cout << "temp: " << temp << std::endl;
            influx_tag_vector.push_back(temp.append(input.str()));
            //std::cout << "590: " << i << " " << influx_tag_vector.back() << std::endl;
            //clear input
            input.str("");
            input << ",up=" << status.channel_up;
            input << ",d_max=" << status.d_fifo_max_words;
            input << ",dma_s=" << dma_stall;
            input << ",desc_s=" << desc_buf_stall;
            input << ",din_full=" << din_full;
            input << ",event_rate=" << event_rate;
            input << ",he=" << status.hard_err;
            input << ",se=" << status.soft_err;
            input << ",eo=" << status.eoe_fifo_overflow;
            input << ",do=" << status.d_fifo_overflow;

            //flib_data,flib=01:00:00,link=0 pci,stall,tans,fill,up,d_max,dma_s,desc_s,din_full,event_rate,he,se,eo,do
            std::string temp2 = influx_base_field_vector.at(j);
            std::string fields = temp2.append(input.str());
            std::string temp3 = influx_tag_vector.at(i);
            db_input_vector.push_back(temp3.append(fields)); 
            //std::cout << "fields: " << fields << std::endl;

            //std::cout << "input back: " << db_input_vector.back() << std::endl;
            //std::cout << "----------------------------------------------------------------------------------------------------------------" << std::endl;
          }
        }
        if (terminal) {

          std::cout << ss.str() << std::endl;

        }

        ++j;

      }

      if (grafana) {

        //building string to post via the http request
        std::stringstream db_input;

        for (unsigned int i = 0; i < db_input_vector.size(); ++i) {

          db_input << db_input_vector.at(i) << "\n";
          //std::cout << db_input_vector.at(i) << std::endl;

        }

        std::string db_input_string = db_input.str();

        //std::cout << "input string: " << std::endl;
        //std::cout << db_input_string << std::endl;

        db_input_vector.clear();

        try {

          http_post(db, db_input_string, http_client_vector.at(0));

        }
        catch (const std::exception& e) {

          std::cout << e.what() << std::endl;
          return EXIT_SUCCESS;

        }

      // sleep will be canceled by signals (which is handy in our case)
      // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66803
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
      ++loop_cnt;
      }
    }
  }
  catch (const std::exception& e) {

    std::cout << "std::exception: " << e.what() << std::endl;

    return EXIT_FAILURE;

  }

  return EXIT_SUCCESS;
}