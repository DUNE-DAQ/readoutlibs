/**
 * @file ReadoutModel.hpp Glue between data source, payload raw processor,
 * latency buffer and request handler.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_READOUTMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_READOUTMODEL_HPP_

//#include "appfwk/app/Nljs.hpp"
#include "coredal/DaqModule.hpp"
#include "coredal/Connection.hpp"
#include "appdal/ReadoutModule.hpp"
#include "appdal/ReadoutModuleConf.hpp"

//#include "appfwk/cmd/Nljs.hpp"
//#include "appfwk/cmd/Structs.hpp"


//#include "appfwk/DAQModuleHelper.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/Receiver.hpp"

#include "logging/Logging.hpp"

#include "opmonlib/InfoCollector.hpp"

#include "daqdataformats/ComponentRequest.hpp"
#include "daqdataformats/Fragment.hpp"

#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TimeSync.hpp"

#include "readoutlibs/ReadoutLogging.hpp"
#include "readoutlibs/concepts/ReadoutConcept.hpp"
#include "appdal/ReadoutModule.hpp"
//#include "readoutlibs/readoutconfig/Nljs.hpp"
#include "readoutlibs/readoutinfo/InfoNljs.hpp"

#include "readoutlibs/FrameErrorRegistry.hpp"

#include "readoutlibs/concepts/LatencyBufferConcept.hpp"
#include "readoutlibs/concepts/RawDataProcessorConcept.hpp"
#include "readoutlibs/concepts/RequestHandlerConcept.hpp"

#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/utils/ReusableThread.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using dunedaq::readoutlibs::logging::TLVL_QUEUE_POP;
using dunedaq::readoutlibs::logging::TLVL_TAKE_NOTE;
using dunedaq::readoutlibs::logging::TLVL_TIME_SYNCS;
using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType, class RequestHandlerType, class LatencyBufferType, class RawDataProcessorType>
class ReadoutModel : public ReadoutConcept
{
public:
  // Using shorter typenames
  using RDT = ReadoutType;
  using RHT = RequestHandlerType;
  using LBT = LatencyBufferType;
  using RPT = RawDataProcessorType;

  // Using timestamp typenames
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)
  static inline constexpr timestamp_t ns = 1;
  static inline constexpr timestamp_t us = 1000 * ns;
  static inline constexpr timestamp_t ms = 1000 * us;
  static inline constexpr timestamp_t s = 1000 * ms;

  // Explicit constructor with run marker pass-through
  explicit ReadoutModel(std::atomic<bool>& run_marker)
    : m_run_marker(run_marker)
    , m_fake_trigger(false)
    , m_current_fake_trigger_id(0)
    , m_send_partial_fragment_if_available(false)
    , m_consumer_thread(0)
    , m_raw_receiver_timeout_ms(0)
    , m_raw_receiver_sleep_us(0)
    , m_raw_data_receiver(nullptr)
    , m_timesync_thread(0)
    , m_latency_buffer_impl(nullptr)
    , m_raw_processor_impl(nullptr)
  {
    m_pid_of_current_process = getpid();
  }

  // Initializes the readoutmodel and its internals
  void init(const appdal::ReadoutModule* modconf);

  // Configures the readoutmodel and its internals
  void conf(const nlohmann::json& args);

  // Unconfigures readoutmodel's internals
  void scrap(const nlohmann::json& args)
  {
    m_request_handler_impl->scrap(args);
    m_latency_buffer_impl->scrap(args);
    m_raw_processor_impl->scrap(args);
  }

  // Starts readoutmodel's internals
  void start(const nlohmann::json& args);

  // Stops readoutmodel's internals
  void stop(const nlohmann::json& args);

  // Record function: invokes request handler's record implementation
  void record(const nlohmann::json& args) override 
  { 
    m_request_handler_impl->record(args); 
  }

  // Opmon get_info call implementation
  void get_info(opmonlib::InfoCollector& ci, int level);

protected:
 
  // Raw data consumer's work function
  void run_consume();

  // Timesync thread's work function
  void run_timesync();

  // Dispatch data request
  void dispatch_requests(dfmessages::DataRequest& data_request);

  // Constuctor params
  std::atomic<bool>& m_run_marker;

  // CONFIGURATION
  //appfwk::app::ModInit m_queue_config;
  bool m_fake_trigger;
  bool m_generate_timesync = false;
  int m_current_fake_trigger_id;
  daqdataformats::SourceID m_sourceid;
  daqdataformats::run_number_t m_run_number;
  bool m_send_partial_fragment_if_available;
  uint64_t m_processing_delay_ticks;
  // STATS
  std::atomic<int> m_num_payloads{ 0 };
  std::atomic<int> m_sum_payloads{ 0 };
  std::atomic<int> m_num_requests{ 0 };
  std::atomic<int> m_sum_requests{ 0 };
  std::atomic<int> m_rawq_timeout_count{ 0 };
  std::atomic<int> m_stats_packet_count{ 0 };
  std::atomic<int> m_num_payloads_overwritten{ 0 };

  // CONSUMER
  ReusableThread m_consumer_thread;

  // RAW RECEIVER
  std::chrono::milliseconds m_raw_receiver_timeout_ms;
  std::chrono::microseconds m_raw_receiver_sleep_us;
  using raw_receiver_ct = iomanager::ReceiverConcept<ReadoutType>;
  std::shared_ptr<raw_receiver_ct> m_raw_data_receiver;

  // REQUEST RECEIVERS
  using request_receiver_ct = iomanager::ReceiverConcept<dfmessages::DataRequest>;
  std::shared_ptr<request_receiver_ct> m_data_request_receiver;

  // FRAGMENT SENDER
  //std::chrono::milliseconds m_fragment_sender_timeout_ms;
  //using fragment_sender_ct = iomanager::SenderConcept<std::pair<std::unique_ptr<daqdataformats::Fragment>, std::string>>;
  //std::shared_ptr<fragment_sender_ct> m_fragment_sender;

  // TIME-SYNC
  using timesync_sender_ct = iomanager::SenderConcept<dfmessages::TimeSync>; // no timeout -> published
  std::shared_ptr<timesync_sender_ct> m_timesync_sender;
  ReusableThread m_timesync_thread;
  std::string m_timesync_connection_name;
  uint32_t m_pid_of_current_process;

  // LATENCY BUFFER
  std::unique_ptr<LatencyBufferType> m_latency_buffer_impl;

  // RAW PROCESSING
  std::unique_ptr<RawDataProcessorType> m_raw_processor_impl;

  // REQUEST HANDLER
  std::unique_ptr<RequestHandlerType> m_request_handler_impl;
  bool m_request_handler_supports_cutoff_timestamp;

  // ERROR REGISTRY
  std::unique_ptr<FrameErrorRegistry> m_error_registry;

  // RUN START T0
  std::chrono::time_point<std::chrono::high_resolution_clock> m_t0;
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/ReadoutModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_READOUTMODEL_HPP_
