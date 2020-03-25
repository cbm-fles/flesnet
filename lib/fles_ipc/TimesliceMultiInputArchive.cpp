// Copyright 2019 Florian Uhlig <f.uhlig@gsi.de>

#include "TimesliceMultiInputArchive.hpp"

#include "StorableTimeslice.hpp"
#include "TimesliceInputArchive.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

/*
  Unexpected behaviour (from tests):

  - Throws boost::filesystem::filesystem_error if given filename does not
  contain a directory (incompatible with plain TimesliceInputArchive)
  - Throws std::out_of_range if given file does not exist because of the
  "stream.at(0)" statement in the constructor
  - Multiple files separated by ";" are merged (sorted by time), while multiple
  files selected by wildcard are visited in sequence
  - Non-standard wildcard matching: Only "*", no "?". No way to protect literal
  "*".
  - Calls "exit()" on errors instead of throwing an exception
*/

namespace filesys = boost::filesystem;

namespace fles {

TimesliceMultiInputArchive::TimesliceMultiInputArchive(
    const std::string& inputString, const std::string& inputDirectory) {

  std::string newInputString;
  if (!inputDirectory.empty()) {
    // split the input string at the character ";" which divides the string
    // into different files/filelists for the different streams
    std::vector<std::string> inputStreams;
    boost::split(inputStreams, inputString, [](char c) { return c == ';'; });
    for (auto& string : inputStreams) {
      newInputString.append(inputDirectory)
          .append("/")
          .append(string)
          .append(";");
    }
    newInputString.pop_back(); // Remove the last ;
  } else {
    newInputString = inputString;
  }

  if (!newInputString.empty()) {
    CreateInputFileList(newInputString);
    for (auto& stream : InputFileList) {
      std::string file = stream.at(0);
      stream.erase(stream.begin());
      source_.push_back(std::unique_ptr<TimesliceInputArchive>(
          new TimesliceInputArchive(file)));
      L_(info) << " Open file: " << file;
    }
  } else {
    L_(fatal) << "No input files defined";
    exit(1);
  }
  InitTimesliceArchive();
}

void TimesliceMultiInputArchive::CreateInputFileList(std::string inputString) {

  // split the input string at the character ";" which divides the string
  // into different files/filelists for the different streams
  std::vector<std::string> inputStreams;
  boost::split(inputStreams, inputString, [](char c) { return c == ';'; });

  // loop over the inputs and extract the file List for
  // the orresponding input
  // The filename should contain the full path to the file
  // The filename can contain the wildcard "*"
  for (auto& string : inputStreams) {
    filesys::path p{string};
    std::string dir = p.parent_path().string();
    std::string filename = p.filename().string();

    std::vector<std::string> v;

    // escape "." which have a special meaning in regex
    // change "*" to ".*" to find any number
    // e.g. tofget4_hd2018.*.tsa => tofget4_hd2018\..*\.tsa
    boost::replace_all(filename, ".", "\\.");
    boost::replace_all(filename, "*", ".*");

    // create regex
    const boost::regex my_filter(filename);

    // loop over all files in input directory
    for (auto&& x : filesys::directory_iterator(p.parent_path())) {
      // Skip if not a file
      if (!boost::filesystem::is_regular_file(x)) {
        continue;
      }
      // Skip if no match
      // x.path().leaf().string() means get from directory iterator the
      // current entry as filesys::path, from this extract the leaf
      // filename or directory name and convert it to a string to be
      // used in the regex:match
      boost::smatch what;
      if (!boost::regex_match(x.path().leaf().string(), what, my_filter)) {
        continue;
      }
      v.push_back(x.path().string());
    }

    // sort the files which match the regex in increasing order
    // (hopefully)
    std::sort(v.begin(), v.end());

    InputFileList.push_back(v);
  }

  // some dubug output
  L_(info) << "Number of input streams: " << InputFileList.size();

  for (auto& streamList : InputFileList) {
    L_(info) << "Number of files in Stream: " << streamList.size();
    for (auto& fileList : streamList) {
      L_(info) << "File: " << fileList;
    }
  }
}

void TimesliceMultiInputArchive::InitTimesliceArchive() {
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

Timeslice* TimesliceMultiInputArchive::do_get() {
  return GetNextTimeslice().release();
}

std::unique_ptr<Timeslice> TimesliceMultiInputArchive::GetNextTimeslice() {

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
      if (!OpenNextFile(currentSource)) {
        // if the first file reaches the end stop reading
        // return std::unique_ptr<const Timeslice>(nullptr);
      } else {
        if ((timeslice = source_.at(currentSource)->get())) {
          sortedSource_.insert({timeslice->index(), currentSource});
          timesliceCont.at(currentSource) = std::move(timeslice);
        }
      }
    }

    return retTimeslice;
  }
  return std::unique_ptr<Timeslice>(nullptr);
}

bool TimesliceMultiInputArchive::OpenNextFile(int element) {
  // First Close and delete existing source
  if (nullptr != source_.at(element)) {
    delete source_.at(element).release();
  }

  if (!InputFileList.at(element).empty()) {
    std::string file = InputFileList.at(element).at(0);
    InputFileList.at(element).erase(InputFileList.at(element).begin());
    source_.at(element) =
        std::unique_ptr<TimesliceInputArchive>(new TimesliceInputArchive(file));
    L_(info) << " Open file: " << file;
  } else {
    L_(info) << "End of files list reached.";
    return false;
  }
  return true;
}

} // namespace fles
