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

#include "appfwk/app/Nljs.hpp"
#include "appfwk/cmd/Nljs.hpp"
#include "appfwk/cmd/Structs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"

#include "logging/Logging.hpp"

#include "opmonlib/InfoCollector.hpp"

#include "daqdataformats/ComponentRequest.hpp"
#include "daqdataformats/Fragment.hpp"

#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TimeSync.hpp"
#include "networkmanager/NetworkManager.hpp"

#include "readoutlibs/ReadoutLogging.hpp"
#include "readoutlibs/concepts/ReadoutConcept.hpp"
#include "readoutlibs/readoutconfig/Nljs.hpp"
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
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)

  static inline constexpr timestamp_t ns = 1;
  static inline constexpr timestamp_t us = 1000 * ns;
  static inline constexpr timestamp_t ms = 1000 * us;
  static inline constexpr timestamp_t s = 1000 * ms;

  explicit ReadoutModel(std::atomic<bool>& run_marker)
    : m_run_marker(run_marker)
    , m_fake_trigger(false)
    , m_current_fake_trigger_id(0)
    , m_consumer_thread(0)
    , m_source_queue_timeout_ms(0)
    , m_raw_data_source(nullptr)
    , m_latency_buffer_impl(nullptr)
    , m_raw_processor_impl(nullptr)
    , m_requester_thread(0)
    , m_timesync_thread(0)
  {
    m_pid_of_current_process = getpid();
  }

  void init(const nlohmann::json& args)
  {
    setup_request_queues(args);

    try {
      auto queue_index = appfwk::queue_index(args, { "raw_input", "fragment_queue" });
      m_raw_data_source.reset(new raw_source_qt(queue_index["raw_input"].inst));
      m_fragment_queue.reset(new fragment_sink_qt(queue_index["fragment_queue"].inst));
    } catch (const ers::Issue& excpt) {
      throw ResourceQueueError(ERS_HERE, "Could not find all necessary queues: raw_input or fragment_queue", "ReadoutModel", excpt);
    }

    // Instantiate functionalities
    m_error_registry.reset(new FrameErrorRegistry());
    m_latency_buffer_impl.reset(new LatencyBufferType());
    m_raw_processor_impl.reset(new RawDataProcessorType(m_error_registry));
    m_request_handler_impl.reset(new RequestHandlerType(m_latency_buffer_impl, m_error_registry));
    m_request_handler_impl->init(args);
    m_raw_processor_impl->init(args);
  }

  void conf(const nlohmann::json& args)
  {
    auto conf = args["readoutmodelconf"].get<readoutconfig::ReadoutModelConf>();
    if (conf.fake_trigger_flag == 0) {
      m_fake_trigger = false;
    } else {
      m_fake_trigger = true;
    }
    m_source_queue_timeout_ms = std::chrono::milliseconds(conf.source_queue_timeout_ms);
    TLOG_DEBUG(TLVL_WORK_STEPS) << "ReadoutModel creation";

    m_geoid.element_id = conf.element_id;
    m_geoid.region_id = conf.region_id;
    m_geoid.system_type = ReadoutType::system_type;

    m_timesync_connection_name = conf.timesync_connection_name;
    m_timesync_topic_name = conf.timesync_topic_name;

    // Configure implementations:
    m_raw_processor_impl->conf(args);
    // Configure the latency buffer before the request handler so the request handler can check for alignment
    // restrictions
    try {
      m_latency_buffer_impl->conf(args);
    } catch (const std::bad_alloc& be) {
      ers::error(ConfigurationError(ERS_HERE, m_geoid, "Latency Buffer can't be allocated with size!"));
    }

    m_request_handler_impl->conf(args);

    // Configure threads:
    m_consumer_thread.set_name("consumer", conf.element_id);
    m_timesync_thread.set_name("timesync", conf.element_id);
    m_requester_thread.set_name("requests", conf.element_id);
  }

  void scrap(const nlohmann::json& args)
  {
    m_request_handler_impl->scrap(args);
    m_latency_buffer_impl->scrap(args);
    m_raw_processor_impl->scrap(args);
  }

  void start(const nlohmann::json& args)
  {
    // Reset opmon variables
    m_sum_payloads = 0;
    m_num_payloads = 0;
    m_sum_requests = 0;
    m_num_requests = 0;
    m_num_payloads_overwritten = 0;
    m_stats_packet_count = 0;
    m_rawq_timeout_count = 0;

    m_t0 = std::chrono::high_resolution_clock::now();

    m_run_number = args.value<dunedaq::daqdataformats::run_number_t>("run", 1);

    TLOG_DEBUG(TLVL_WORK_STEPS) << "Starting threads...";
    m_raw_processor_impl->start(args);
    m_request_handler_impl->start(args);
    m_consumer_thread.set_work(
      &ReadoutModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType>::run_consume, this);
    m_requester_thread.set_work(
      &ReadoutModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType>::run_requests, this);
    m_timesync_thread.set_work(
      &ReadoutModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType>::run_timesync, this);
  }

  void stop(const nlohmann::json& args)
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Stoppping threads...";
    m_request_handler_impl->stop(args);
    while (!m_timesync_thread.get_readiness()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (!m_consumer_thread.get_readiness()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (!m_requester_thread.get_readiness()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Flushing latency buffer with occupancy: " << m_latency_buffer_impl->occupancy();
    m_latency_buffer_impl->flush();
    m_raw_processor_impl->stop(args);
    m_raw_processor_impl->reset_last_daq_time();
  }

  void record(const nlohmann::json& args) override { m_request_handler_impl->record(args); }

  void get_info(opmonlib::InfoCollector& ci, int level)
  {
    readoutinfo::ReadoutInfo ri;
    ri.sum_payloads = m_sum_payloads.load();
    ri.num_payloads = m_num_payloads.exchange(0);
    ri.sum_requests = m_sum_requests.load();
    ri.num_requests = m_num_requests.exchange(0);
    ri.num_payloads_overwritten = m_num_payloads_overwritten.exchange(0);
    ri.num_buffer_elements = m_latency_buffer_impl->occupancy();

    auto now = std::chrono::high_resolution_clock::now();
    int new_packets = m_stats_packet_count.exchange(0);
    double seconds = std::chrono::duration_cast<std::chrono::microseconds>(now - m_t0).count() / 1000000.;
    TLOG_DEBUG(TLVL_TAKE_NOTE) << "Consumed Packet rate: " << std::to_string(new_packets / seconds / 1000.) << " [kHz]";
    auto rawq_timeouts = m_rawq_timeout_count.exchange(0);
    if (rawq_timeouts > 0) {
      TLOG_DEBUG(TLVL_TAKE_NOTE) << "***ERROR: Raw input queue timed out " << std::to_string(rawq_timeouts)
                                 << " times!";
    }
    m_t0 = now;

    ri.rate_payloads_consumed = new_packets / seconds / 1000.;
    ri.num_raw_queue_timeouts = rawq_timeouts;

    ci.add(ri);

    m_request_handler_impl->get_info(ci, level);
    m_raw_processor_impl->get_info(ci, level);
  }

private:
  void setup_request_queues(const nlohmann::json& args)
  {
    auto queue_index = appfwk::queue_index(args, {});
    size_t index = 0;
    while (queue_index.find("data_requests_" + std::to_string(index)) != queue_index.end()) {
      m_data_request_queues.push_back(
        std::make_unique<request_source_qt>(queue_index["data_requests_" + std::to_string(index)].inst));
      index++;
    }
  }

  void run_consume()
  {
    m_rawq_timeout_count = 0;
    m_num_payloads = 0;
    m_sum_payloads = 0;
    m_stats_packet_count = 0;

    TLOG_DEBUG(TLVL_WORK_STEPS) << "Consumer thread started...";
    while (m_run_marker.load() || m_raw_data_source->can_pop()) {
      ReadoutType payload;
      // Try to acquire data
      try {
        m_raw_data_source->pop(payload, m_source_queue_timeout_ms);
        m_raw_processor_impl->preprocess_item(&payload);
        if (!m_latency_buffer_impl->write(std::move(payload))) {
          TLOG_DEBUG(TLVL_TAKE_NOTE) << "***ERROR: Latency buffer is full and data was overwritten!";
          m_num_payloads_overwritten++;
        }
        m_raw_processor_impl->postprocess_item(m_latency_buffer_impl->back());
        ++m_num_payloads;
        ++m_sum_payloads;
        ++m_stats_packet_count;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        ++m_rawq_timeout_count;
        // ers::error(QueueTimeoutError(ERS_HERE, " raw source "));
      }
    }
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Consumer thread joins... ";
  }

  void run_timesync()
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "TimeSync thread started...";
    m_num_requests = 0;
    m_sum_requests = 0;
    uint64_t msg_seqno = 0;
    auto once_per_run = true;
    while (m_run_marker.load()) {
      try {
        auto timesyncmsg = dfmessages::TimeSync(m_raw_processor_impl->get_last_daq_time());
        if (timesyncmsg.daq_time != 0) {
          timesyncmsg.run_number = m_run_number;
          timesyncmsg.sequence_number = ++msg_seqno;
          timesyncmsg.source_pid = m_pid_of_current_process;
          TLOG_DEBUG(TLVL_TIME_SYNCS) << "New timesync: daq=" << timesyncmsg.daq_time
                                      << " wall=" << timesyncmsg.system_time << " run=" << timesyncmsg.run_number
                                      << " seqno=" << timesyncmsg.sequence_number << " pid=" << timesyncmsg.source_pid;
          try {
            auto serialised_timesync = dunedaq::serialization::serialize(timesyncmsg, dunedaq::serialization::kMsgPack);
            networkmanager::NetworkManager::get().send_to(m_timesync_connection_name,
                                                          static_cast<const void*>(serialised_timesync.data()),
                                                          serialised_timesync.size(),
                                                          std::chrono::milliseconds(500),
                                                          m_timesync_topic_name);
          } catch (ers::Issue& excpt) {
            ers::warning(
              TimeSyncTransmissionFailed(ERS_HERE, m_geoid, m_timesync_connection_name, m_timesync_topic_name, excpt));
          }
  
            
          if (m_fake_trigger) {
            dfmessages::DataRequest dr;
            ++m_current_fake_trigger_id;
            dr.trigger_number = m_current_fake_trigger_id;
            dr.trigger_timestamp = timesyncmsg.daq_time > 500 * us ? timesyncmsg.daq_time - 500 * us : 0;
            auto width = 300000;
            uint offset = 100;
            dr.request_information.window_begin = dr.trigger_timestamp > offset ? dr.trigger_timestamp - offset : 0;
            dr.request_information.window_end = dr.request_information.window_begin + width;
            dr.request_information.component = m_geoid;
            TLOG_DEBUG(TLVL_WORK_STEPS) << "Issuing fake trigger based on timesync. "
                                        << " ts=" << dr.trigger_timestamp << " window_begin=" << dr.request_information.window_begin
                                        << " window_end=" << dr.request_information.window_end;
            m_request_handler_impl->issue_request(dr, *m_fragment_queue);
            ++m_num_requests;
            ++m_sum_requests;
          }
        } else {
          if (once_per_run) {
            TLOG() << "Timesync with DAQ time 0 won't be sent out as it's an invalid sync.";
            once_per_run = false;
          }
        }
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        // ++m_timesyncqueue_timeout;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    once_per_run = true;
    TLOG_DEBUG(TLVL_WORK_STEPS) << "TimeSync thread joins...";
  }

  void run_requests()
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Requester thread started...";
    m_num_requests = 0;
    m_sum_requests = 0;
    dfmessages::DataRequest data_request;

    while (m_run_marker.load()) {
      bool popped_element = false;
      for (size_t i = 0; i < m_data_request_queues.size(); ++i) {
        auto& request_source = *m_data_request_queues[i];
        try {
          request_source.pop(data_request, std::chrono::milliseconds(0));
          popped_element = true;
          if (data_request.request_information.component != m_geoid) {
            ers::error(RequestGeoIDMismatch(ERS_HERE, m_geoid, data_request.request_information.component));
            return;
          }
          m_request_handler_impl->issue_request(data_request, *m_fragment_queue);
          ++m_num_requests;
          ++m_sum_requests;
          TLOG_DEBUG(TLVL_QUEUE_POP) << "Received DataRequest for trigger_number " << data_request.trigger_number
                                     << ", run number " << data_request.run_number << " (APA number "
                                     << m_geoid.region_id << ", link number " << m_geoid.element_id << ")";
        } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
          // not an error, safe to continue
        }
      }
      if (!popped_element) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    // Clear queues
    for (auto& queue : m_data_request_queues) {
      while (queue->can_pop()) {
        queue->pop(data_request, m_source_queue_timeout_ms);
      }
    }
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Requester thread joins... ";
  }

  // Constuctor params
  std::atomic<bool>& m_run_marker;

  // CONFIGURATION
  appfwk::app::ModInit m_queue_config;
  bool m_fake_trigger;
  int m_current_fake_trigger_id;
  daqdataformats::GeoID m_geoid;
  daqdataformats::run_number_t m_run_number;

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

  // RAW SOURCE
  std::chrono::milliseconds m_source_queue_timeout_ms;
  using raw_source_qt = appfwk::DAQSource<ReadoutType>;
  std::unique_ptr<raw_source_qt> m_raw_data_source;

  // REQUEST SOURCES
  std::chrono::milliseconds m_request_queue_timeout_ms;
  using request_source_qt = appfwk::DAQSource<dfmessages::DataRequest>;
  std::vector<std::unique_ptr<request_source_qt>> m_data_request_queues;

  // FRAGMENT SINK
  std::chrono::milliseconds m_fragment_queue_timeout_ms;
  using fragment_sink_qt = appfwk::DAQSink<std::pair<std::unique_ptr<daqdataformats::Fragment>, std::string>>;
  std::unique_ptr<fragment_sink_qt> m_fragment_queue;

  // LATENCY BUFFER:
  std::unique_ptr<LatencyBufferType> m_latency_buffer_impl;

  // RAW PROCESSING:
  std::unique_ptr<RawDataProcessorType> m_raw_processor_impl;

  // REQUEST HANDLER:
  std::unique_ptr<RequestHandlerType> m_request_handler_impl;
  ReusableThread m_requester_thread;

  std::unique_ptr<FrameErrorRegistry> m_error_registry;

  // TIME-SYNC
  ReusableThread m_timesync_thread;
  std::string m_timesync_connection_name;
  std::string m_timesync_topic_name;
  uint32_t m_pid_of_current_process;

  std::chrono::time_point<std::chrono::high_resolution_clock> m_t0;
};

} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_READOUTMODEL_HPP_
