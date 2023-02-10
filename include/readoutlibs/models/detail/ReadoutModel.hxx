// Declarations for ReadoutModel

namespace dunedaq {
namespace readoutlibs {

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::init(const nlohmann::json& args)
{
  // Setupo request queues
  setup_request_queues(args);

  try {
    auto ini = args.get<appfwk::app::ModInit>();
    for (const auto &cr : ini.conn_refs) {
      if (cr.name == "raw_input") {
        TLOG() << "Create raw_input receiver";
  	    m_raw_data_receiver = get_iom_receiver<RDT>(cr);
      } else if (cr.name == "timesync_output") {
  	    TLOG() << "Create timesync sender";
  	    m_timesync_sender = get_iom_sender<dfmessages::TimeSync>(cr);
      }
    }
    //iomanager::ConnectionRef raw_input_ref = iomanager::ConnectionRef{ "input", "raw_input", iomanager::Direction::kInput };
    //m_raw_data_receiver = get_iom_receiver<RDT>(ini["raw_input"]);
    //iomanager::ConnectionRef frag_output_ref = iomanager::ConnectionRef{ "output", "frag_output", iomanager::Direction::kOutput };
    //iomanager::ConnectionRef timesync_output_ref = iomanager::ConnectionRef{ "output", "timesync_output", iomanager::Direction::kOutput };
    //m_timesync_sender = get_iom_sender<dfmessages::TimeSync>("timesync_output");
  } catch (const ers::Issue& excpt) {
    throw ResourceQueueError(ERS_HERE, "raw_input or frag_output", "ReadoutModel", excpt);
  }

  std::string errstring = "";
  if (m_raw_data_receiver == nullptr) {
    errstring = "raw_input";
  }
  if (m_timesync_sender == nullptr) {
    if (errstring != "") { 
      errstring += ", "; 
    }
    errstring += "timesync_output";
  }
  if (m_data_request_receiver == nullptr)
  {
    if (errstring != "") { 
      errstring += ", "; 
    }
    errstring += "request_input";
  }
  if (errstring != "") {
    throw ResourceQueueError(ERS_HERE, errstring, "ReadoutModel");
  }

  // Instantiate functionalities
  m_error_registry.reset(new FrameErrorRegistry());
  m_latency_buffer_impl.reset(new LBT());
  m_raw_processor_impl.reset(new RPT(m_error_registry));
  m_request_handler_impl.reset(new RHT(m_latency_buffer_impl, m_error_registry));
  m_request_handler_impl->init(args);
  m_raw_processor_impl->init(args);
}

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::conf(const nlohmann::json& args)
{
  auto conf = args["readoutmodelconf"].get<readoutconfig::ReadoutModelConf>();
  if (conf.fake_trigger_flag == 0) {
    m_fake_trigger = false;
  } else {
    m_fake_trigger = true;
  }
  m_raw_receiver_timeout_ms = std::chrono::milliseconds(conf.source_queue_timeout_ms);
  TLOG_DEBUG(TLVL_WORK_STEPS) << "ReadoutModel creation";

  m_sourceid.id = conf.source_id;
  m_sourceid.subsystem = RDT::subsystem;

  m_timesync_connection_name = conf.timesync_connection_name;
  m_timesync_topic_name = conf.timesync_topic_name;

  m_send_partial_fragment_if_available = conf.send_partial_fragment_if_available;

  // Configure implementations:
  m_raw_processor_impl->conf(args);
  // Configure the latency buffer before the request handler so the request handler can check for alignment
  // restrictions
  try {
    m_latency_buffer_impl->conf(args);
  } catch (const std::bad_alloc& be) {
    ers::error(ConfigurationError(ERS_HERE, m_sourceid, "Latency Buffer can't be allocated with size!"));
  }

  m_request_handler_impl->conf(args);

  // Configure threads:
  m_consumer_thread.set_name("consumer", conf.source_id);
  m_timesync_thread.set_name("timesync", conf.source_id);
}


template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::start(const nlohmann::json& args)
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
  m_consumer_thread.set_work(&ReadoutModel<RDT, RHT, LBT, RPT>::run_consume, this);
  m_timesync_thread.set_work(&ReadoutModel<RDT, RHT, LBT, RPT>::run_timesync, this);
  // Register callback to receive and dispatch data requests
  m_data_request_receiver->add_callback(
    std::bind(&ReadoutModel<RDT, RHT, LBT, RPT>::dispatch_requests, this, std::placeholders::_1));
}

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::stop(const nlohmann::json& args)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Stoppping threads...";

  // Stop receiving data requests as first thing
  m_data_request_receiver->remove_callback();
  TLOG() << "Sbhuller:" << "SourceID " << m_sourceid << "Removed request reciever callback";
  // Stop the other threads
  m_request_handler_impl->stop(args);
  while (!m_timesync_thread.get_readiness()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  TLOG() << "Sbhuller:" << "SourceID " << m_sourceid << "Timesync threads completed";
  while (!m_consumer_thread.get_readiness()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  TLOG() << "Sbhuller:" << "SourceID " << m_sourceid << "consumer threads completed";
  TLOG() << "Sbhuller:" << "SourceID " << m_sourceid << " Flushing latency buffer with occupancy: " << m_latency_buffer_impl->occupancy();
  m_latency_buffer_impl->flush();
  TLOG() << "Sbhuller:" << "SourceID " << m_sourceid << "Latency buffer flushed";
  m_raw_processor_impl->stop(args);
  TLOG() << "Sbhuller:" << "SourceID " << m_sourceid << "RDP stopped";
  m_raw_processor_impl->reset_last_daq_time();
}

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::get_info(opmonlib::InfoCollector& ci, int level)
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
    TLOG_DEBUG(TLVL_TAKE_NOTE) << "***ERROR: Raw input queue timed out " 
      << std::to_string(rawq_timeouts) << " times!";
  }
  m_t0 = now;

  ri.rate_payloads_consumed = new_packets / seconds / 1000.;
  ri.num_raw_queue_timeouts = rawq_timeouts;

  ci.add(ri);

  m_request_handler_impl->get_info(ci, level);
  m_raw_processor_impl->get_info(ci, level);
}

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::setup_request_queues(const nlohmann::json& args)
{
  auto ini = args.get<appfwk::app::ModInit>();	  
  for (const auto& cr : ini.conn_refs) {
    if(cr.name == "request_input") {
      m_data_request_receiver = get_iom_receiver<dfmessages::DataRequest>(cr) ;
    }
  }
  //int index = 0;
  //iomanager::ConnectionRef request_input_ref = iomanager::ConnectionRef{ "input", "request_input_*", iomanager::Direction::kInput };
  // Loop over request_input refs...
  //while (queue_index.find("data_requests_" + std::to_string(index)) != queue_index.end()) {
  //  m_data_request_receivers.push_back( get_iom_receiver<dfmessages::DataRequest>(ini.conn_refs["request_input"]) );
    //index++;
  //}
}

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::run_consume()
{
  m_rawq_timeout_count = 0;
  m_num_payloads = 0;
  m_sum_payloads = 0;
  m_stats_packet_count = 0;

  TLOG() << "Sbhuller: Consumer thread started...";
  while (m_run_marker.load()) {
    // Try to acquire data
    try {
      RDT payload = m_raw_data_receiver->receive(m_raw_receiver_timeout_ms);
      m_raw_processor_impl->preprocess_item(&payload);
      if (!m_latency_buffer_impl->write(std::move(payload))) {
        TLOG() << "Sbhuller:" << "SourceID " << m_sourceid << " ***ERROR: Latency buffer is full and data was overwritten!";
        m_num_payloads_overwritten++;
      }
      m_raw_processor_impl->postprocess_item(m_latency_buffer_impl->back());
      ++m_num_payloads;
      ++m_sum_payloads;
      ++m_stats_packet_count;
    } catch (const iomanager::TimeoutExpired& excpt) {
      ++m_rawq_timeout_count;
      TLOG() << "Sbhuller" << "SourceID " << m_sourceid << ": timeout expired!";
      // ers::error(QueueTimeoutError(ERS_HERE, " raw source "));
    }
  }
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Consumer thread joins... ";
}

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::run_timesync()
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "TimeSync thread started...";
  m_num_requests = 0;
  m_sum_requests = 0;
  uint64_t msg_seqno = 0;
  timestamp_t prev_timestamp = 0;
  auto once_per_run = true;
  while (m_run_marker.load()) {
    try {
      auto timesyncmsg = dfmessages::TimeSync(m_raw_processor_impl->get_last_daq_time());
      // daq_time is zero for the first received timesync, and may
      // be the same as the previous daq_time if the data has
      // stopped flowing. In both cases we don't send the TimeSync
      if (timesyncmsg.daq_time != 0 && timesyncmsg.daq_time != prev_timestamp) {
        prev_timestamp = timesyncmsg.daq_time;
        timesyncmsg.run_number = m_run_number;
        timesyncmsg.sequence_number = ++msg_seqno;
        timesyncmsg.source_pid = m_pid_of_current_process;
        TLOG_DEBUG(TLVL_TIME_SYNCS) << "New timesync: daq=" << timesyncmsg.daq_time
          << " wall=" << timesyncmsg.system_time << " run=" << timesyncmsg.run_number
          << " seqno=" << timesyncmsg.sequence_number << " pid=" << timesyncmsg.source_pid;
        try {
            dfmessages::TimeSync timesyncmsg_copy(timesyncmsg);
          m_timesync_sender->send(std::move(timesyncmsg_copy), std::chrono::milliseconds(500), m_timesync_topic_name);
        } catch (ers::Issue& excpt) {
          ers::warning(
            TimeSyncTransmissionFailed(ERS_HERE, m_sourceid, m_timesync_connection_name, m_timesync_topic_name, excpt));
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
          dr.request_information.component = m_sourceid;
          dr.data_destination = "data_fragments_q";
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Issuing fake trigger based on timesync. "
            << " ts=" << dr.trigger_timestamp 
            << " window_begin=" << dr.request_information.window_begin
            << " window_end=" << dr.request_information.window_end;
          m_request_handler_impl->issue_request(dr, false);

          ++m_num_requests;
          ++m_sum_requests;
        }
      } else {
        if (once_per_run) {
          TLOG() << "Timesync with DAQ time 0 won't be sent out as it's an invalid sync.";
          once_per_run = false;
        }
      }
    } catch (const iomanager::TimeoutExpired& excpt) {
      // ++m_timesyncqueue_timeout;
    }
    // Split up the 100ms sleep into 10 sleeps of 10ms, so we respond to "stop" quicker
    for (size_t i=0; i<10; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (!m_run_marker.load()) {
        break;
      }
    }
  }
  once_per_run = true;
  TLOG_DEBUG(TLVL_WORK_STEPS) << "TimeSync thread joins...";
}

template<class RDT, class RHT, class LBT, class RPT>
void 
ReadoutModel<RDT, RHT, LBT, RPT>::dispatch_requests(dfmessages::DataRequest& data_request)
{
  if (data_request.request_information.component != m_sourceid) {
     ers::error(RequestSourceIDMismatch(ERS_HERE, m_sourceid, data_request.request_information.component));
     return;
  }
  TLOG_DEBUG(TLVL_QUEUE_POP) << "Received DataRequest" 
    << " for trig/seq_number " << data_request.trigger_number << "." << data_request.sequence_number
    << ", runno " << data_request.run_number
    << ", trig timestamp " << data_request.trigger_timestamp
    << ", SourceID: " << data_request.request_information.component
    << ", window begin/end " << data_request.request_information.window_begin
    << "/" << data_request.request_information.window_end
    << ", dest: " << data_request.data_destination;
  m_request_handler_impl->issue_request(data_request, m_send_partial_fragment_if_available);
  ++m_num_requests;
  ++m_sum_requests;
}

} // namespace readoutlibs
} // namespace dunedaq
