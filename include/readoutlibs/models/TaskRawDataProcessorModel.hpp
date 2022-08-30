/**
 * @file TaskRawDataProcessorModel.hpp Raw processor parallel task and pipeline
 * combination using a vector of std::functions
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_TASKRAWDATAPROCESSORMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_TASKRAWDATAPROCESSORMODEL_HPP_

#include "daqdataformats/SourceID.hpp"
#include "logging/Logging.hpp"
#include "readoutlibs/FrameErrorRegistry.hpp"
#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/ReadoutLogging.hpp"
#include "readoutlibs/concepts/RawDataProcessorConcept.hpp"
#include "readoutlibs/readoutconfig/Nljs.hpp"
#include "readoutlibs/utils/ReusableThread.hpp"

#include <folly/ProducerConsumerQueue.h>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
class TaskRawDataProcessorModel : public RawDataProcessorConcept<ReadoutType>
{
public:
  // Excplicit constructor with error registry
  explicit TaskRawDataProcessorModel(std::unique_ptr<FrameErrorRegistry>& error_registry)
    : RawDataProcessorConcept<ReadoutType>()
    , m_error_registry(error_registry)
  {}

  // Destructor
  ~TaskRawDataProcessorModel() {}

  // Configures the element pointer queue for the post-processors, and the GeoID
  void conf(const nlohmann::json& cfg) override;

  // Clears elements to process, pre-proc pipeline, and post-proc functions
  void scrap(const nlohmann::json& /*cfg*/) override;

  // Starts the pre-processor pipeline and the parallel post-processor threads
  void start(const nlohmann::json& /*args*/) override;

  // Stops the pre-processor pipeline and the parallel post-processor threads
  void stop(const nlohmann::json& /*args*/) override;

  // Generic get_info, currently empty
  virtual void get_info(opmonlib::InfoCollector& /*ci*/, int /*level*/)
  {
    // No stats for now, extend later
  }

  // Resets last known/processed DAQ timestamp
  void reset_last_daq_time() { m_last_processed_daq_ts.store(0); }

  // Returns last processed ReadoutTyped element's DAQ timestamp
  std::uint64_t get_last_daq_time() override { return m_last_processed_daq_ts.load(); } // NOLINT(build/unsigned)

  // Registers ReadoutType item pointer to the pre-processing pipeline
  void preprocess_item(ReadoutType* item) override { invoke_all_preprocess_functions(item); }

  // Registers ReadoutType item pointer to to the post-processing queue
  void postprocess_item(const ReadoutType* item) override;

  // Registers a pre-processing task to the pre-processor pipeline
  template<typename Task>
  void add_preprocess_task(Task&& task);

  // Registers a post-processing task to the parallel post-processors
  template<typename Task>
  void add_postprocess_task(Task&& task);

  // Invokes all preprocessor functions as pipeline
  void invoke_all_preprocess_functions(ReadoutType* item);

  // Launches all preprocessor functions as async
  void launch_all_preprocess_functions(ReadoutType* item);

protected:
  // Post-processing thread runner
  void run_post_processing_thread(std::function<void(const ReadoutType*)>& function,
                                  folly::ProducerConsumerQueue<const ReadoutType*>& queue);

  // Run marker
  std::atomic<bool> m_run_marker{ false };

  // Pre-processing pipeline functions
  std::vector<std::function<void(ReadoutType*)>> m_preprocess_functions;
  std::unique_ptr<FrameErrorRegistry>& m_error_registry;

  // Post-processing functions and their corresponding threads
  std::vector<std::function<void(const ReadoutType*)>> m_post_process_functions;
  std::vector<std::unique_ptr<folly::ProducerConsumerQueue<const ReadoutType*>>> m_items_to_postprocess_queues;
  std::vector<std::unique_ptr<ReusableThread>> m_post_process_threads;

  // Internals
  size_t m_postprocess_queue_sizes;
  uint32_t m_this_link_number; // NOLINT(build/unsigned)
  daqdataformats::SourceID m_sourceid;
  bool m_emulator_mode{ false };
  std::atomic<std::uint64_t> m_last_processed_daq_ts{ 0 }; // NOLINT(build/unsigned)

};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/TaskRawDataProcessorModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_TASKRAWDATAPROCESSORMODEL_HPP_
