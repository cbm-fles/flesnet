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
  TimesliceAutoSource(const std::string& locator);

  /**
   * \brief Construct a TimesliceAutoSource object and initialize the
   * actual source class(es)
   *
   * \param locators The addresses of the input sources to read data from as a
   * vector of strings
   */
  TimesliceAutoSource(const std::vector<std::string>& locators);

  /// Delete copy constructor (non-copyable).
  TimesliceAutoSource(const TimesliceAutoSource&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const TimesliceAutoSource&) = delete;

  bool eos() const override { return source_->eos(); }

  ~TimesliceAutoSource() override = default;

private:
  std::unique_ptr<TimesliceSource> source_;

  void init(const std::vector<std::string>& locators);

  Timeslice* do_get() override { return source_->get().release(); }
};

} // namespace fles
