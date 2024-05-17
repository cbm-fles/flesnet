// Copyright 2021 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::AutoSource template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "InputArchive.hpp"
#include "InputArchiveLoop.hpp"
#include "InputArchiveSequence.hpp"
#include "ItemWorkerProtocol.hpp"
#include "MergingSource.hpp"
#include "Source.hpp"
#include "Subscriber.hpp"
#include "System.hpp"
#include "TimesliceReceiver.hpp"
#include "Utility.hpp"

#include <memory>
#include <string>

namespace fles {

/**
 * \brief The AutoSource base class implements the generic
 * item-based input interface. It is a wrapper around the actual source
 * classes and provides convenience functions for flexible initialization of
 * different source classes through locator strings.
 *
 * This class creates internally one or more Source objects of the
 * following types:
 * - InputArchive
 * - InputArchiveSequence
 * - Subscriber
 *
 * If there is more than one Source object, these objects are handed
 * over to an instance of the MergingSource class, which internally merges data
 * from the sources in ascending order of item index. <b>Note that this
 * merging is a debugging feature that will generally not be available in online
 * operation.</b>
 *
 * The list of locator strings is parsed according to the following rules:
 * - Each provided locator string corresponds to at least one Source
 * object.
 * - If the string starts with `tcp://`, it is considered an address string used
 * to initialize a Subscriber object.
 * - If the string starts with `file://` or does not contain `://` at all, it is
 * considered a local filepath. The filepath may be relative or absolute, and it
 * may contain standard wildcard characters and patterns as understood by the
 * POSIX glob() function. It may also contain the special string `"%%n"` as a
 * placeholder for the file sequence number (starting at `"0000"`) as generated
 * by the OutputArchiveSequence class.
 * - Glob patterns in the filepath are expanded at initialization, resulting in
 * a list of filepaths. If the original filepath contains a `"%%n"` placeholder,
 * a separate InputArchiveSequence instance is created for each
 * resulting filepath. If there is no `%n` placeholder, the resulting filepaths
 * are handed over collectively to a InputArchiveSequence instance for
 * sequential input, or, if there is only a single resulting filepath, a
 * InputArchive instance.
 *
 * ## Examples
 * \code
 * 1. AutoSource("example.tsa")
 * 2. AutoSource("example_%n.tsa")
 * 3. AutoSource("*.tsa")
 * 4. AutoSource("tcp://localhost:5556")
 * 5. AutoSource("example_node?_%n.tsa")
 * 6. AutoSource({"example0.tsa", "example1.tsa"})
 * 7. AutoSource("{example0.tsa,example1.tsa}")
 * \endcode
 *
 * These examples will result in the creation of the following objects:
 * 1. A single InputArchive
 * 2. A single InputArchiveSequence
 * 3. A single InputArchiveSequence (if the`"*"` wildcard expands to
 *    more than one instance)
 * 4. A single Subscriber
 * 5. A MergingSource containing InputArchiveSequence objects (if the
 *    `"?"` wildcard expands to more than one instance)
 * 6. A MergingSource containing two InputArchive objects
 * 7. A single InputArchiveSequence

 */
template <class Base, class Storable, class View, ArchiveType archive_type>
class AutoSource : public Source<Base> {
public:
  /**
   * \brief Construct an AutoSource object and initialize the
   * actual source class(es).
   *
   * \param locator The address of the input source(s) to read data from as a
   * string. Semicolon (";") characters in the string are treated as separators.
   */
  AutoSource(const std::string& locator) {
    // As a first step, treat ";" characters in the locator string as separators
    init(split(locator, ";"));
  }

  /**
   * \brief Construct an AutoSource object and initialize the
   * actual source class(es).
   *
   * \param locators The address(es) of the input source(s) to read data from as
   * a vector of strings. No special treatment of the semicolon (";") character.
   */
  AutoSource(const std::vector<std::string>& locators) { init(locators); }

  /// Delete copy constructor (non-copyable).
  AutoSource(const AutoSource&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const AutoSource&) = delete;

  /**
   * \brief Return the end-of-stream state of the source.
   *
   * \return Returns true if all managed sources are in the end-of-stream state,
   * false otherwise.
   */
  [[nodiscard]] bool eos() const override { return source_->eos(); }

  ~AutoSource() override = default;

private:
  using TInputArchive = InputArchive<Base, Storable, archive_type>;
  using TInputArchiveLoop = InputArchiveLoop<Base, Storable, archive_type>;
  using TInputArchiveSequence =
      InputArchiveSequence<Base, Storable, archive_type>;

  std::unique_ptr<Source<Base>> source_;

  void init(const std::vector<std::string>& locators) {
    std::vector<std::unique_ptr<Source<Base>>> sources;

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
            std::unique_ptr<Source<Base>> source =
                std::make_unique<TInputArchiveSequence>(path);
            sources.emplace_back(std::move(source));
          }
        } else {
          if (paths.size() == 1) {
            if (cycles == 1) {
              std::unique_ptr<Source<Base>> source =
                  std::make_unique<TInputArchive>(paths.front());
              sources.emplace_back(std::move(source));
            } else {
              std::unique_ptr<Source<Base>> source =
                  std::make_unique<TInputArchiveLoop>(paths.front(), cycles);
              sources.emplace_back(std::move(source));
            }
          } else if (paths.size() > 1) {
            std::unique_ptr<Source<Base>> source =
                std::make_unique<TInputArchiveSequence>(paths);
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
        std::unique_ptr<Source<Base>> source =
            std::make_unique<Subscriber<Base, Storable>>(address, hwm);
        sources.emplace_back(std::move(source));

      } else if (uri.scheme == "shm") {
        WorkerParameters param{1, 0, WorkerQueuePolicy::QueueAll, 0,
                               "AutoSource at PID " +
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
          } else if (key == "group") {
            param.group_id = std::stoull(value);
          } else {
            throw std::runtime_error(
                "query parameter not implemented for scheme " + uri.scheme +
                ": " + key);
          }
        }
        const auto ipc_identifier = uri.authority + uri.path;
        std::unique_ptr<Source<Base>> source =
            std::make_unique<Receiver<Base, View>>(ipc_identifier, param);
        sources.emplace_back(std::move(source));

      } else {
        throw std::runtime_error("scheme not implemented: " + uri.scheme);
      }
    }

    if (sources.size() == 1) {
      source_ = std::move(sources.front());
    } else if (sources.size() > 1) {
      source_ =
          std::make_unique<MergingSource<Source<Base>>>(std::move(sources));
    }
  }

  Base* do_get() override { return source_->get().release(); }
};

} // namespace fles
