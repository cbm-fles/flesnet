// Copyright 2019 Florian Uhlig <f.uhlig@gsi.de>

#include "TimesliceMultiSubscriber.hpp"

#include "StorableTimeslice.hpp"
#include "TimesliceSubscriber.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>

namespace fles {

TimesliceMultiSubscriber::TimesliceMultiSubscriber(
    const std::string& inputString, uint32_t hwm) {
  if (!inputString.empty()) {
    CreateHostPortFileList(inputString);
    for (auto& stream : InputHostPortList) {
      std::string server = stream;
      source_.push_back(std::unique_ptr<TimesliceSubscriber>(
          new TimesliceSubscriber(server, hwm)));
      L_(info) << " Open server: " << server << " with ZMQ HW mark " << hwm;
    }
  } else {
    L_(fatal) << "No server defined";
    exit(1);
  }
  InitTimesliceSubscriber();
}

void TimesliceMultiSubscriber::CreateHostPortFileList(std::string inputString) {

  // split the input string at the character ";" which devides the string
  // into different file/filelists for the different streams
  std::vector<std::string> inputStreams;
  boost::split(inputStreams, inputString, [](char c) { return c == ';'; });

  // loop over the inputs and extract for each input the host address including
  // eventual port if not port is defined, add the default port The hostname
  // cannot contain the wildcard "*"
  for (auto& string : inputStreams) {

    if (string.empty()) {
      L_(error) << " Empty hostname string, ignoring it";
    }

    std::vector<std::string> stringsHostnamePort;
    boost::split(stringsHostnamePort, string, [](char c) { return c == ':'; });

    switch (stringsHostnamePort.size()) {
    case 1:
      string += DefaultPort;
      break;
    case 2:
      break;
    default:
      // Bad hostname, ignore it
      L_(error) << " Bad hostname string: " << string;
      continue;
    }
    std::string fullpath = "tcp://";
    fullpath += string;
    InputHostPortList.push_back(fullpath);
  }

  // some dubug output
  L_(info) << "Number of input streams: " << InputHostPortList.size();

  for (auto& streamList : InputHostPortList) {
    L_(info) << "Host and port: " << streamList;
  }
}

void TimesliceMultiSubscriber::InitTimesliceSubscriber() {
  timesliceCont.resize(source_.size());

  int element = 0;
  for (auto& sourceNr : source_) {
    if (auto timeslice = sourceNr->get()) {
      sortedSource_.insert({timeslice->index(), element});
      timesliceCont.at(element) = std::move(timeslice);
      element++;
    } else {
      L_(fatal) << "Could not read a timeslice from input stream " << element;
      exit(1);
    }
  }
}

Timeslice* TimesliceMultiSubscriber::do_get() {
  return GetNextTimeslice().release();
}

std::unique_ptr<Timeslice> TimesliceMultiSubscriber::GetNextTimeslice() {

  if (!sortedSource_.empty()) {
    // take the first element from the set which is the one with the smallest
    // ts number
    // (*(sortedSource_.begin())) dereference the std::set iterator to get
    // access to the contained pair afterwards erase the first element of the
    // set
    int currentSource = (*(sortedSource_.begin())).second;
    sortedSource_.erase(sortedSource_.begin());

    std::unique_ptr<Timeslice> retTimeslice =
        std::move(timesliceCont.at(currentSource));

    if (auto timeslice = source_.at(currentSource)->get()) {
      sortedSource_.insert({timeslice->index(), currentSource});
      timesliceCont.at(currentSource) = std::move(timeslice);
    } else {
      // When any server stopped sending, stop reading
      // return std::unique_ptr<const Timeslice>(nullptr);
    }

    return retTimeslice;
  }
  return std::unique_ptr<Timeslice>(nullptr);
}

} // end of namespace fles
