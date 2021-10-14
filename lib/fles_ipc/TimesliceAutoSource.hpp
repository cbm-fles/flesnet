// Copyright 2021 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::TimesliceAutoSource class type.
#pragma once

#include "TimesliceSource.hpp"

namespace fles {

/**
 * \brief The TimesliceAutoSource base class implements the generic
 * timeslice-based input interface. It is a wrapper around the actual source
 * classes and provides convenience functions for flexible initialization of
 * different source classes through locator strings.
 *
 * This class creates internally one or more TimesliceSource objects of the
 * following types:
 * - TimesliceInputArchive
 * - TimesliceInputArchiveSequence
 * - TimesliceSubsciber
 *
 * If there is more than one TimesliceSource object, these objects are handed
 * over to an instance of the MergingSource class, which internally merges data
 * from the sources in ascending order of timeslice index. <b>Note that this
 * merging is a debugging feature that will generally not be available in online
 * operation.</b>
 *
 * The list of locator strings is parsed according to the following rules:
 * - Each provided locator string corresponds to at least one TimesliceSource
 * object.
 * - If the string starts with `tcp://`, it is considered an address string used
 * to initialize a TimesliceSubscriber object.
 * - If the string starts with `file://` or does not contain `://` at all, it is
 * considered a local filepath. The filepath may be relative or absolute, and it
 * may contain standard wildcard characters and patterns as understood by the
 * POSIX glob() function. It may also contain the special string `"%%n"` as a
 * placeholder for the file sequence number (starting at `"0000"`) as generated
 * by the TimesliceOutputArchiveSequence class.
 * - Glob patterns in the filepath are expanded at initialization, resulting in
 * a list of filepaths. If the original filepath contains a `"%%n"` placeholder,
 * a separate TimesliceInputArchiveSequence instance is created for each
 * resulting filepath. If there is no `%n` placeholder, the resulting filepaths
 * are handed over collectively to a TimesliceInputArchiveSequence instance for
 * sequential input, or, if there is only a single resulting filepath, a
 * TimesliceInputArchive instance.
 *
 * ## Examples
 * \code
 * 1. TimesliceAutoSource("example.tsa")
 * 2. TimesliceAutoSource("example_%n.tsa")
 * 3. TimesliceAutoSource("*.tsa")
 * 4. TimesliceAutoSource("tcp://localhost:5556")
 * 5. TimesliceAutoSource("example_node?_%n.tsa")
 * 6. TimesliceAutoSource({"example0.tsa", "example1.tsa"})
 * 7. TimesliceAutoSource("{example0.tsa,example1.tsa}")
 * \endcode
 *
 * These examples will result in the creation of the following objects:
 * 1. A single TimesliceInputArchive
 * 2. A single TimesliceInputArchiveSequence
 * 3. A single TimesliceInputArchiveSequence (if the`"*"` wildcard expands to
 *    more than one instance)
 * 4. A single TimesliceSubscriber
 * 5. A MergingSource containing TimesliceInputArchiveSequence objects (if the
 *    `"?"` wildcard expands to more than one instance)
 * 6. A MergingSource containing two TimesliceInputArchive objects
 * 7. A single TimesliceInputArchiveSequence

 */
class TimesliceAutoSource : public TimesliceSource {
public:
  /**
   * \brief Construct a TimesliceAutoSource object and initialize the
   * actual source class(es).
   *
   * \param locator The address of the input source(s) to read data from as a
   * string. Semicolon (";") characters in the string are treated as separators.
   */
  TimesliceAutoSource(const std::string& locator);

  /**
   * \brief Construct a TimesliceAutoSource object and initialize the
   * actual source class(es).
   *
   * \param locators The address(es) of the input source(s) to read data from as
   * a vector of strings. No special treatment of the semicolon (";") character.
   */
  TimesliceAutoSource(const std::vector<std::string>& locators);

  /// Delete copy constructor (non-copyable).
  TimesliceAutoSource(const TimesliceAutoSource&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const TimesliceAutoSource&) = delete;

  /**
   * \brief Return the end-of-stream state of the source.
   *
   * \return Returns true if all managed sources are in the end-of-stream state,
   * false otherwise.
   */
  [[nodiscard]] bool eos() const override { return source_->eos(); }

  ~TimesliceAutoSource() override = default;

private:
  std::unique_ptr<TimesliceSource> source_;

  void init(const std::vector<std::string>& locators);

  Timeslice* do_get() override { return source_->get().release(); }
};

} // namespace fles
