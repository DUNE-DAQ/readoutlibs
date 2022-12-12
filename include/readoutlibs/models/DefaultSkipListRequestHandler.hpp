/**
 * @file DefaultSkipListRequestHandler.hpp Trigger matching mechanism 
 * used for skip list based LBs in readout models
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_DEFAULTSKIPLISTREQUESTHANDLER_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_DEFAULTSKIPLISTREQUESTHANDLER_HPP_

#include "logging/Logging.hpp"

#include "readoutlibs/FrameErrorRegistry.hpp"
#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/ReadoutLogging.hpp"
#include "readoutlibs/models/DefaultRequestHandlerModel.hpp"
#include "readoutlibs/models/SkipListLatencyBufferModel.hpp"

#include <atomic>
#include <memory>
#include <string>

using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace readoutlibs {

template<class T>
class DefaultSkipListRequestHandler 
  : public readoutlibs::DefaultRequestHandlerModel<T, readoutlibs::SkipListLatencyBufferModel<T>>
{
public:
  using inherited = readoutlibs::DefaultRequestHandlerModel<T, readoutlibs::SkipListLatencyBufferModel<T>>;
  using SkipListAcc = typename folly::ConcurrentSkipList<T>::Accessor;
  using SkipListSkip = typename folly::ConcurrentSkipList<T>::Skipper;

  // Constructor that binds LB and error registry
  DefaultSkipListRequestHandler(std::unique_ptr<readoutlibs::SkipListLatencyBufferModel<T>>& latency_buffer,
                                std::unique_ptr<readoutlibs::FrameErrorRegistry>& error_registry)
    : readoutlibs::DefaultRequestHandlerModel<T, readoutlibs::SkipListLatencyBufferModel<T>>(
        latency_buffer,
        error_registry)
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "DefaultSkipListRequestHandler created...";
  }

protected:
  // Default ceanup request override
  void cleanup() override { skip_list_cleanup_request(); }

  // Cleanup request implementation for skiplist LB
  void skip_list_cleanup_request(); 

private:
  // Constants
  static const constexpr uint64_t m_max_ts_diff = 10000000; // NOLINT(build/unsigned)

  // Stats
  std::atomic<int> m_found_requested_count{ 0 };
  std::atomic<int> m_bad_requested_count{ 0 };
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/DefaultSkipListRequestHandler.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_DEFAULTSKIPLISTREQUESTHANDLER_HPP_
