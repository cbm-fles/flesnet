// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Application.hpp"
#include "Verificator.hpp"
#include "log.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace std;
Application::Application(Parameters const& par) : par_(par) {}

Application::~Application() {}

void Application::run() {
  Verificator val(par_.max_threads);
  bool valid = false;
  uint64_t components_cnt = par_.input_archives.size();
  uint64_t timeslice_cnt = par_.timeslice_cnt;


  if (par_.tsa_archives.size() == 2) {
    if (val.find_tsa_b_in_a(par_.tsa_archives[0], par_.tsa_archives[1])) {
      L_(info) << "Archives valid" << endl;
    } else {
      throw runtime_error("Archives NOT VALID!");
    }
  } else if (par_.check_tsa_forwarding) {
    if (!val.verify_ts_forwarding(par_.input_archives, par_.output_archives)) {
          throw runtime_error("Archives NOT VALID!");
    };
    L_(info) << "Archives valid" << endl;
    return;
  } else {
    if (!par_.skip_metadata) {
      valid = val.verify_ts_metadata(par_.output_archives, &timeslice_cnt, par_.timeslice_size, par_.overlap, components_cnt);
      if (!valid) {
        throw runtime_error("Output metadata check FAILED. Archives NOT VALID!");
      }
    }

    valid = val.verify_forward(par_.input_archives, par_.output_archives, timeslice_cnt, par_.overlap);
    if (!valid) {
      throw runtime_error("Forward varification FAILED. Archives NOT VALID!");
    }
  }
}
