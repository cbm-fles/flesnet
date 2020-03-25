// Copyright 2019 Florian Uhlig <f.uhlig@gsi.de>
/// \file
/// \brief Defines the fles::TimesliceMultiSubscriber class.
#pragma once

#include "StorableTimeslice.hpp"
#include "TimesliceSource.hpp"
#include "log.hpp"
#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fles {
/**
 * \brief The TimesliceMultiSubscriber class receives serialized timeslice data
 * from several zeromq socket and returns the timeslice with the smallest index.
 */
class TimesliceMultiSubscriber : public TimesliceSource {
public:
  /// Construct timeslice subscriber receiving from given ZMQ address.
  explicit TimesliceMultiSubscriber(const std::string& /*inputString*/,
                                    uint32_t hwm = 1);

  /// Delete copy constructor (non-copyable).
  TimesliceMultiSubscriber(const TimesliceMultiSubscriber&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const TimesliceMultiSubscriber&) = delete;

  ~TimesliceMultiSubscriber() override = default;

  /**
   * \brief Retrieve the next item.
   *
   * This function blocks if no next item is yet available from all stream.
   *
   * \return pointer to the item, or nullptr if no more
   * timeslices available in the input archives
   */
  std::unique_ptr<Timeslice> get() { return (GetNextTimeslice()); };

  bool eos() const override { return sortedSource_.empty(); }

private:
  Timeslice* do_get() override;

  void InitTimesliceSubscriber();
  void CreateHostPortFileList(std::string /*inputString*/);
  std::unique_ptr<Timeslice> GetNextTimeslice();

  std::vector<std::unique_ptr<TimesliceSource>> source_;

  std::string DefaultPort = ":5556";
  std::vector<std::string> InputHostPortList;

  std::vector<std::unique_ptr<Timeslice>> timesliceCont;

  std::set<std::pair<uint64_t, int>> sortedSource_;

  logging::OstreamLog status_log_{status};
  logging::OstreamLog debug_log_{debug};
};

} // namespace fles
