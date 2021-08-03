// Copyright 2021 Jan de Cuveland <cmail@cuveland.de>
#include "TimesliceAutoSource.hpp"

#include "MergingSource.hpp"
#include "System.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceSubscriber.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>

#include <memory>

namespace fles {

TimesliceAutoSource::TimesliceAutoSource(const std::string& locator) {
  // As a first step, treat ";" characters in the locator string as separators
  std::vector<std::string> locators;
  boost::split(locators, locator, [](char c) { return c == ';'; });
  init(locators);
}

TimesliceAutoSource::TimesliceAutoSource(
    const std::vector<std::string>& locators) {
  init(locators);
}

void TimesliceAutoSource::init(const std::vector<std::string>& locators) {
  std::vector<std::unique_ptr<fles::TimesliceSource>> sources;

  for (const auto& locator : locators) {
    // Check if locator has URI pattern
    std::string protocol;
    std::string host_path = locator;
    auto separator = locator.find("://", 0);
    if (separator != std::string::npos) {
      protocol = locator.substr(0, separator);
      host_path = locator.substr(separator + 3);
    }

    if (protocol == "file" || protocol.empty()) {
      // Find pathnames matching a pattern.
      //
      // The sequence number placeholder "%n" is expanded to the first valid
      // value of "0000" before glob'ing and replaced back afterwards. This
      // will not work if the pathname contains both the placeholder and the
      // string "0000". Nonexistant files are catched already at this stage by
      // glob() throwing a runtime_error.
      auto paths =
          system::glob(boost::replace_all_copy(host_path, "%n", "0000"));
      if (host_path.find("%n") != std::string::npos) {
        for (auto& path : paths) {
          boost::replace_all(path, "0000", "%n");
          std::unique_ptr<fles::TimesliceSource> source =
              std::make_unique<fles::TimesliceInputArchiveSequence>(path);
          sources.emplace_back(std::move(source));
        }
      } else {
        if (paths.size() == 1) {
          std::unique_ptr<fles::TimesliceSource> source =
              std::make_unique<fles::TimesliceInputArchive>(paths.front());
          sources.emplace_back(std::move(source));
        } else if (paths.size() > 1) {
          std::unique_ptr<fles::TimesliceSource> source =
              std::make_unique<fles::TimesliceInputArchiveSequence>(paths);
          sources.emplace_back(std::move(source));
        }
      }
    } else if (protocol == "tcp") {
      std::unique_ptr<fles::TimesliceSource> source =
          std::make_unique<fles::TimesliceSubscriber>(locator);
      sources.emplace_back(std::move(source));
    } else {
      throw std::runtime_error("protocol not implemented: " + protocol);
    }
  }

  if (sources.size() == 1) {
    source_ = std::move(sources.front());
  } else if (sources.size() > 1) {
    source_ = std::make_unique<MergingSource<fles::TimesliceSource>>(
        std::move(sources));
  }
}

} // namespace fles
