// Copyright 2021 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceAutoSource class type.
#pragma once

#include "MergingSource.hpp"
#include "System.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceSource.hpp"
#include "TimesliceSubscriber.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>

namespace fles {

/**
 * \brief The TimesliceAutoSource base class implements the generic
 * timeslice-based input interface. It is a wrapper around the actual source
 * classes and provides convenience functions for flexible initialization of
 * different source classes through locator strings.
 *
 *
 */
class TimesliceAutoSource : public TimesliceSource {
public:
  /**
   * \brief Construct a TimesliceAutoSource object and initialize the
   * actual source class(es)
   *
   * \param locator The address of the input source(s) to read data from as a
   * string
   */
  TimesliceAutoSource(const std::string& locator) {
    // As a first step, treat ";" characters in the locator string as separators
    std::vector<std::string> locators;
    boost::split(locators, locator, [](char c) { return c == ';'; });
    init(locators);
  }

  /**
   * \brief Construct a TimesliceAutoSource object and initialize the
   * actual source class(es)
   *
   * \param locators The addresses of the input sources to read data from as a
   * vector of strings
   */
  TimesliceAutoSource(const std::vector<std::string>& locators) {
    init(locators);
  }

  /// Delete copy constructor (non-copyable).
  TimesliceAutoSource(const TimesliceAutoSource&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const TimesliceAutoSource&) = delete;

  bool eos() const override { return source_->eos(); }

  ~TimesliceAutoSource() override = default;

private:
  std::unique_ptr<TimesliceSource> source_;

  void init(const std::vector<std::string>& locators) {
    std::vector<std::unique_ptr<fles::TimesliceSource>> sources;

    std::vector<std::string> archive_pathnames;

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
          for (auto& path : paths) {
            std::unique_ptr<fles::TimesliceSource> source =
                std::make_unique<fles::TimesliceInputArchive>(path);
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

  Timeslice* do_get() override { return source_->get().release(); }
};

} // namespace fles
