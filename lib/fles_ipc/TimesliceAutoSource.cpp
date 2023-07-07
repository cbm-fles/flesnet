// Copyright 2021 Jan de Cuveland <cmail@cuveland.de>
#include "TimesliceAutoSource.hpp"

#include "ItemWorkerProtocol.hpp"
#include "MergingSource.hpp"
#include "System.hpp"
#include "TimesliceInputArchive.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceSubscriber.hpp"
#include "Utility.hpp"

#include <memory>
#include <string>

namespace fles {

TimesliceAutoSource::TimesliceAutoSource(const std::string& locator) {
  // As a first step, treat ";" characters in the locator string as separators
  init(split(locator, ";"));
}

TimesliceAutoSource::TimesliceAutoSource(
    const std::vector<std::string>& locators) {
  init(locators);
}

void TimesliceAutoSource::init(const std::vector<std::string>& locators) {
  std::vector<std::unique_ptr<fles::TimesliceSource>> sources;

  for (const auto& locator : locators) {
    // If locator has no full URI pattern, everything is in "uri.path"
    UriComponents uri{locator};

    if (uri.scheme == "file" || uri.scheme.empty()) {
      uint64_t cycles = 1;
      for (auto& [key, value] : uri.query_components) {
        if (key == "cycles") {
          cycles = stoull(value);
        } else {
          throw std::runtime_error(
              "query parameter not implemented for scheme file: " + key);
        }
      }
      const auto file_path = uri.authority + uri.path;

      // Find pathnames matching a pattern.
      //
      // The sequence number placeholder "%n" is expanded to the first valid
      // value of "0000" before glob'ing and replaced back afterwards. This
      // will not work if the pathname contains both the placeholder and the
      // string "0000". Nonexistant files are caught already at this stage by
      // glob() throwing a runtime_error.
      auto paths = system::glob(replace_all_copy(file_path, "%n", "0000"));
      if (file_path.find("%n") != std::string::npos) {
        for (auto& path : paths) {
          replace_all(path, "0000", "%n");
          std::unique_ptr<fles::TimesliceSource> source =
              std::make_unique<fles::TimesliceInputArchiveSequence>(path);
          sources.emplace_back(std::move(source));
        }
      } else {
        if (paths.size() == 1) {
          if (cycles == 1) {
            std::unique_ptr<fles::TimesliceSource> source =
                std::make_unique<fles::TimesliceInputArchive>(paths.front());
            sources.emplace_back(std::move(source));
          } else {
            std::unique_ptr<fles::TimesliceSource> source =
                std::make_unique<fles::TimesliceInputArchiveLoop>(paths.front(),
                                                                  cycles);
            sources.emplace_back(std::move(source));
          }
        } else if (paths.size() > 1) {
          std::unique_ptr<fles::TimesliceSource> source =
              std::make_unique<fles::TimesliceInputArchiveSequence>(paths);
          sources.emplace_back(std::move(source));
        }
      }

    } else if (uri.scheme == "tcp") {
      uint32_t hwm = 1;
      for (auto& [key, value] : uri.query_components) {
        if (key == "hwm") {
          hwm = stou(value);
        } else {
          throw std::runtime_error(
              "query parameter not implemented for scheme " + uri.scheme +
              ": " + key);
        }
      }
      const auto address = uri.scheme + "://" + uri.authority;
      std::unique_ptr<fles::TimesliceSource> source =
          std::make_unique<fles::TimesliceSubscriber>(address, hwm);
      sources.emplace_back(std::move(source));

    } else if (uri.scheme == "shm") {
      WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll,
                             "TimesliceAutoSource at PID " +
                                 std::to_string(system::current_pid())};
      for (auto& [key, value] : uri.query_components) {
        if (key == "stride") {
          param.stride = std::stoull(value);
        } else if (key == "offset") {
          param.offset = std::stoull(value);
        } else if (key == "queue") {
          static const std::map<std::string, WorkerQueuePolicy> queue_map = {
              {"all", WorkerQueuePolicy::QueueAll},
              {"one", WorkerQueuePolicy::PrebufferOne},
              {"skip", WorkerQueuePolicy::Skip}};
          param.queue_policy = queue_map.at(value);
        } else {
          throw std::runtime_error(
              "query parameter not implemented for scheme " + uri.scheme +
              ": " + key);
        }
      }
      const auto ipc_identifier = uri.authority + uri.path;
      std::unique_ptr<fles::TimesliceSource> source =
          std::make_unique<fles::TimesliceReceiver>(ipc_identifier, param);
      sources.emplace_back(std::move(source));

    } else {
      throw std::runtime_error("scheme not implemented: " + uri.scheme);
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
