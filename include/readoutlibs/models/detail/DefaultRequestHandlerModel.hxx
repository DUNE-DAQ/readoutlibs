// Declarations for DefaultRequestHandlerModel

namespace dunedaq {
namespace readoutlibs {

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::conf(const nlohmann::json& args)
{
  auto conf = args["requesthandlerconf"].get<readoutconfig::RequestHandlerConf>();
  m_pop_limit_pct = conf.pop_limit_pct;
  m_pop_size_pct = conf.pop_size_pct;
  m_buffer_capacity = conf.latency_buffer_size;
  m_num_request_handling_threads = conf.num_request_handling_threads;
  m_request_timeout_ms = conf.request_timeout_ms;
  m_output_file = conf.output_file;
  m_sourceid.id = conf.source_id;
  m_sourceid.subsystem = RDT::subsystem;
  m_detid = conf.det_id;
  m_stream_buffer_size = conf.stream_buffer_size;
  m_warn_on_timeout = conf.warn_on_timeout;
  m_warn_about_empty_buffer = conf.warn_about_empty_buffer;
  // if (m_configured) {
  //  ers::error(ConfigurationError(ERS_HERE, "This object is already configured!"));
  if (m_pop_limit_pct < 0.0f || m_pop_limit_pct > 1.0f || m_pop_size_pct < 0.0f || m_pop_size_pct > 1.0f) {
    ers::error(ConfigurationError(ERS_HERE, m_sourceid, "Auto-pop percentage out of range."));
  } else {
    m_pop_limit_size = m_pop_limit_pct * m_buffer_capacity;
    m_max_requested_elements = m_pop_limit_size - m_pop_limit_size * m_pop_size_pct;
  }

  if (conf.enable_raw_recording && !m_recording_configured) {
    std::string output_file = conf.output_file;
    if (remove(output_file.c_str()) == 0) {
      TLOG(TLVL_WORK_STEPS) << "Removed existing output file from previous run: " << conf.output_file << std::endl;
    }

    m_buffered_writer.open(conf.output_file, conf.stream_buffer_size, conf.compression_algorithm, conf.use_o_direct);
    m_recording_configured = true;
  }

  m_recording_thread.set_name("recording", conf.source_id);
  m_cleanup_thread.set_name("cleanup", conf.source_id);

  std::ostringstream oss;
  oss << "RequestHandler configured. " << std::fixed << std::setprecision(2)
      << "auto-pop limit: " << m_pop_limit_pct * 100.0f << "% "
      << "auto-pop size: " << m_pop_size_pct * 100.0f << "% "
      << "max requested elements: " << m_max_requested_elements;
  TLOG_DEBUG(TLVL_WORK_STEPS) << oss.str();
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::scrap(const nlohmann::json& /*args*/)
{
  if (m_buffered_writer.is_open()) {
    m_buffered_writer.close();
  }
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::start(const nlohmann::json& /*args*/)
{
  // Reset opmon variables
  m_num_requests_found = 0;
  m_num_requests_bad = 0;
  m_num_requests_old_window = 0;
  m_num_requests_delayed = 0;
  m_num_requests_uncategorized = 0;
  m_num_buffer_cleanups = 0;
  m_num_requests_timed_out = 0;
  m_handled_requests = 0;
  m_response_time_acc = 0;
  m_pop_reqs = 0;
  m_pops_count = 0;
  m_payloads_written = 0;

  m_t0 = std::chrono::high_resolution_clock::now();

  m_request_handler_thread_pool = std::make_unique<boost::asio::thread_pool>(m_num_request_handling_threads);

  m_run_marker.store(true);
  m_cleanup_thread.set_work(&DefaultRequestHandlerModel<RDT, LBT>::periodic_cleanups, this);
  m_waiting_queue_thread = 
    std::thread(&DefaultRequestHandlerModel<RDT, LBT>::check_waiting_requests, this);
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::stop(const nlohmann::json& /*args*/)
{
  m_run_marker.store(false);
  // if (m_recording) throw CommandError(ERS_HERE, "Recording is still ongoing!");
  while (!m_recording_thread.get_readiness()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  while (!m_cleanup_thread.get_readiness()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  m_waiting_queue_thread.join();
  m_request_handler_thread_pool->join();
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::record(const nlohmann::json& args)
{
  auto conf = args.get<readoutconfig::RecordingParams>();
  if (m_recording.load()) {
    ers::error(CommandError(ERS_HERE, m_sourceid, "A recording is still running, no new recording was started!"));
    return;
  } else if (!m_buffered_writer.is_open()) {
    ers::error(CommandError(ERS_HERE, m_sourceid, "DLH is not configured for recording"));
    return;
  }
  m_recording_thread.set_work(
    [&](int duration) {
      TLOG() << "Start recording for " << duration << " second(s)" << std::endl;
      m_recording.exchange(true);
      auto start_of_recording = std::chrono::high_resolution_clock::now();
      auto current_time = start_of_recording;
      m_next_timestamp_to_record = 0;
      RDT element_to_search;
      while (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_of_recording).count() < duration) {
        if (!m_cleanup_requested || (m_next_timestamp_to_record == 0)) {
          if (m_next_timestamp_to_record == 0) {
            auto front = m_latency_buffer->front();
            m_next_timestamp_to_record = front == nullptr ? 0 : front->get_first_timestamp();
          }
          element_to_search.set_first_timestamp(m_next_timestamp_to_record);
          size_t processed_chunks_in_loop = 0;

          {
            std::unique_lock<std::mutex> lock(m_cv_mutex);
            m_cv.wait(lock, [&] { return !m_cleanup_requested; });
            m_requests_running++;
          }
          m_cv.notify_all();
          auto chunk_iter = m_latency_buffer->lower_bound(element_to_search, true);
          auto end = m_latency_buffer->end();
          {
            std::lock_guard<std::mutex> lock(m_cv_mutex);
            m_requests_running--;
          }
          m_cv.notify_all();

          for (; chunk_iter != end && chunk_iter.good() && processed_chunks_in_loop < 1000;) {
            if ((*chunk_iter).get_first_timestamp() >= m_next_timestamp_to_record) {
              if (!m_buffered_writer.write(reinterpret_cast<char*>(chunk_iter->begin()), // NOLINT
                                           chunk_iter->get_payload_size())) {
                ers::warning(CannotWriteToFile(ERS_HERE, m_output_file));
              }
              m_payloads_written++;
              processed_chunks_in_loop++;
              m_next_timestamp_to_record = (*chunk_iter).get_first_timestamp() +
                                           RDT::expected_tick_difference * (*chunk_iter).get_num_frames();
            }
            ++chunk_iter;
          }
        }
        current_time = std::chrono::high_resolution_clock::now();
      }
      m_next_timestamp_to_record = std::numeric_limits<uint64_t>::max(); // NOLINT (build/unsigned)

      TLOG() << "Stop recording" << std::endl;
      m_recording.exchange(false);
      m_buffered_writer.flush();
    },
    conf.duration);
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::cleanup_check()
{
  std::unique_lock<std::mutex> lock(m_cv_mutex);
  if (m_latency_buffer->occupancy() > m_pop_limit_size && !m_cleanup_requested.exchange(true)) {
    m_cv.wait(lock, [&] { return m_requests_running == 0; });
    cleanup();
    m_cleanup_requested = false;
    m_cv.notify_all();
  }
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::issue_request(dfmessages::DataRequest datarequest,
                                                    bool send_partial_fragment_if_available)
{
  boost::asio::post(*m_request_handler_thread_pool, [&, send_partial_fragment_if_available, datarequest]() { // start a thread from pool
    auto t_req_begin = std::chrono::high_resolution_clock::now();
    {
      std::unique_lock<std::mutex> lock(m_cv_mutex);
      m_cv.wait(lock, [&] { return !m_cleanup_requested; });
      m_requests_running++;
    }
    m_cv.notify_all();
    auto result = data_request(datarequest, send_partial_fragment_if_available);
    {
      std::lock_guard<std::mutex> lock(m_cv_mutex);
      m_requests_running--;
    }
    m_cv.notify_all();
    if (result.result_code == ResultCode::kFound || result.result_code == ResultCode::kNotFound) {
      try { // Send to fragment connection
        TLOG_DEBUG(TLVL_QUEUE_PUSH) << "Sending fragment with trigger/sequence_number "
          << result.fragment->get_trigger_number() << "."
          << result.fragment->get_sequence_number() << ", run number "
          << result.fragment->get_run_number() << ", and SourceID "
          << result.fragment->get_element_id();
        // Send fragment
        get_iom_sender<std::unique_ptr<daqdataformats::Fragment>>(datarequest.data_destination)
          ->send(std::move(result.fragment), std::chrono::milliseconds(10));

      } catch (const ers::Issue& excpt) {
        ers::warning(CannotWriteToQueue(ERS_HERE, m_sourceid, "fragment queue", excpt));
      }
    } else if (result.result_code == ResultCode::kNotYet) {
      TLOG_DEBUG(TLVL_WORK_STEPS) << "Re-queue request. "
                                  << "With timestamp=" << result.data_request.trigger_timestamp;
      std::lock_guard<std::mutex> wait_lock_guard(m_waiting_requests_lock);
      m_waiting_requests.push_back(RequestElement(datarequest, std::chrono::high_resolution_clock::now(),
                                                  send_partial_fragment_if_available));
    }
    auto t_req_end = std::chrono::high_resolution_clock::now();
    auto us_req_took = std::chrono::duration_cast<std::chrono::microseconds>(t_req_end - t_req_begin);
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Responding to data request took: " << us_req_took.count() << "[us]";
    // if (result.result_code == ResultCode::kFound) {
    //   std::lock_guard<std::mutex> time_lock_guard(m_response_time_log_lock);
    //   m_response_time_log.push_back( std::make_pair<int, int>(result.data_request.trigger_number,
    //   us_req_took.count()) );
    // }
    m_response_time_acc.fetch_add(us_req_took.count());
    m_handled_requests++;
  });
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  readoutinfo::RequestHandlerInfo info;
  info.num_requests_found = m_num_requests_found.exchange(0);
  info.num_requests_bad = m_num_requests_bad.exchange(0);
  info.num_requests_old_window = m_num_requests_old_window.exchange(0);
  info.num_requests_delayed = m_num_requests_delayed.exchange(0);
  info.num_requests_uncategorized = m_num_requests_uncategorized.exchange(0);
  info.num_buffer_cleanups = m_num_buffer_cleanups.exchange(0);
  info.num_requests_waiting = m_waiting_requests.size();
  info.num_requests_timed_out = m_num_requests_timed_out.exchange(0);
  info.is_recording = m_recording;
  info.num_payloads_written = m_payloads_written.exchange(0);
  info.recording_status = m_recording ? "Y" : "N";

  int new_pop_reqs = 0;
  int new_pop_count = 0;
  int new_occupancy = 0;
  int handled_requests = m_handled_requests.exchange(0);
  int response_time_total = m_response_time_acc.exchange(0);
  auto now = std::chrono::high_resolution_clock::now();
  new_pop_reqs = m_pop_reqs.exchange(0);
  new_pop_count = m_pops_count.exchange(0);
  new_occupancy = m_occupancy;
  double seconds = std::chrono::duration_cast<std::chrono::microseconds>(now - m_t0).count() / 1000000.;
  TLOG_DEBUG(TLVL_HOUSEKEEPING) << "Cleanup request rate: " << new_pop_reqs / seconds / 1. << " [Hz]"
                                << " Dropped: " << new_pop_count << " Occupancy: " << new_occupancy;

  // std::unique_lock<std::mutex> time_lorunck_guard(m_response_time_log_lock);
  // if (!m_response_time_log.empty()) {
  //  std::ostringstream oss;
  // oss << "Completed data requests [trig id, took us]: ";
  //  while (!m_response_time_log.empty()) {
  // for (int i = 0; i < m_response_time_log.size(); ++i) {
  //    auto& fr = m_response_time_log.front();
  //    ++new_request_count;
  //    new_request_times += fr.second;
  // oss << fr.first << ". in " << fr.second << " | ";
  //    m_response_time_log.pop_front();
  //}
  // TLOG_DEBUG(TLVL_HOUSEKEEPING) << oss.str();
  // TLOG_DEBUG(TLVL_HOUSEKEEPING) << "Completed requests: " << new_request_count
  //                                << " | Avarage response time: " << new_request_times / new_request_count <<
  //                                "[us]";
  //}
  // time_lock_guard.unlock();
  if (handled_requests > 0) {
    TLOG_DEBUG(TLVL_HOUSEKEEPING) << "Completed requests: " << handled_requests
                                  << " | Avarage response time: " << response_time_total / handled_requests << "[us]";
    info.avg_request_response_time = response_time_total / handled_requests;
  }

  m_t0 = now;

  ci.add(info);
}


template<class RDT, class LBT>
std::unique_ptr<daqdataformats::Fragment> 
DefaultRequestHandlerModel<RDT, LBT>::create_empty_fragment(const dfmessages::DataRequest& dr)
{
  auto frag_header = create_fragment_header(dr);
  frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
  auto fragment = std::make_unique<daqdataformats::Fragment>(std::vector<std::pair<void*, size_t>>());
  fragment->set_header_fields(frag_header);
  return fragment;
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::periodic_cleanups()
{
  while (m_run_marker.load()) {
    cleanup_check();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::cleanup()
{
  // auto now_s = time::now_as<std::chrono::seconds>();
  auto size_guess = m_latency_buffer->occupancy();
  if (size_guess > m_pop_limit_size) {
    ++m_pop_reqs;
    unsigned to_pop = m_pop_size_pct * m_latency_buffer->occupancy();

    unsigned popped = 0;
    for (size_t i = 0; i < to_pop; ++i) {
      if (m_latency_buffer->front()->get_first_timestamp() < m_next_timestamp_to_record) {
        m_latency_buffer->pop(1);
        popped++;
      } else {
        break;
      }
    }
    // m_pops_count += to_pop;
    m_occupancy = m_latency_buffer->occupancy();
    m_pops_count += popped;
    m_error_registry->remove_errors_until(m_latency_buffer->front()->get_first_timestamp());
  }
  m_num_buffer_cleanups++;
}

template<class RDT, class LBT>
void 
DefaultRequestHandlerModel<RDT, LBT>::check_waiting_requests()
{
  // At run stop, we wait until all waiting requests have either:
  //
  // 1. been serviced because an item past the end of the window arrived in the buffer
  // 2. timed out by going past m_request_timeout_ms, and returned a partial fragment
  while (m_run_marker.load() || m_waiting_requests.size() > 0) {
    {
      std::lock_guard<std::mutex> lock_guard(m_waiting_requests_lock);

      auto last_frame = m_latency_buffer->back();                                       // NOLINT
      uint64_t newest_ts = last_frame == nullptr ? std::numeric_limits<uint64_t>::min() // NOLINT(build/unsigned)
                                                 : last_frame->get_first_timestamp();

      size_t size = m_waiting_requests.size();

      for (size_t i = 0; i < size;) {
        if (m_waiting_requests[i].request.request_information.window_end < newest_ts) {
          issue_request(m_waiting_requests[i].request, m_waiting_requests[i].send_partial_fragment_if_available);
          std::swap(m_waiting_requests[i], m_waiting_requests.back());
          m_waiting_requests.pop_back();
          size--;
        } else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_waiting_requests[i].start_time).count() >= m_request_timeout_ms) {
          issue_request(m_waiting_requests[i].request, true);

          if (m_warn_on_timeout) {
            ers::warning(dunedaq::readoutlibs::VerboseRequestTimedOut(ERS_HERE, m_sourceid,
                                                                      m_waiting_requests[i].request.trigger_number,
                                                                      m_waiting_requests[i].request.sequence_number,
                                                                      m_waiting_requests[i].request.run_number,
                                                                      m_waiting_requests[i].request.request_information.window_begin,
                                                                      m_waiting_requests[i].request.request_information.window_end,
                                                                      m_waiting_requests[i].request.data_destination));
          }

          m_num_requests_bad++;
          m_num_requests_timed_out++;

          std::swap(m_waiting_requests[i], m_waiting_requests.back());
          m_waiting_requests.pop_back();
          size--;
        } else {
          i++;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

template<class RDT, class LBT>
std::vector<std::pair<void*, size_t>> 
DefaultRequestHandlerModel<RDT, LBT>::get_fragment_pieces(uint64_t start_win_ts,
                                                          uint64_t end_win_ts,
                                                          RequestResult& rres)
{
  std::vector<std::pair<void*, size_t>> frag_pieces;
  RDT request_element = RDT();
  request_element.set_first_timestamp(start_win_ts);
  auto start_iter = m_error_registry->has_error("MISSING_FRAMES")
                      ? m_latency_buffer->lower_bound(request_element, true)
                      : m_latency_buffer->lower_bound(request_element, false);
  if (start_iter == m_latency_buffer->end()) {
    // Due to some concurrent access, the start_iter could not be retrieved successfully, try again
    ++m_num_requests_delayed;
    rres.result_code = ResultCode::kNotYet; // give it another chance
  } else {
    rres.result_code = ResultCode::kFound;
    ++m_num_requests_found;

    auto elements_handled = 0;

    RDT* element = &(*start_iter);
    while (start_iter.good() && element->get_first_timestamp() < end_win_ts) {
      if ( element->get_first_timestamp() + (element->get_num_frames() - 1) * RDT::expected_tick_difference < start_win_ts) {
        // skip processing for current element, out of readout window.
      } else if (
         (element->get_first_timestamp() < start_win_ts &&
          element->get_first_timestamp() + (element->get_num_frames() - 1) * RDT::expected_tick_difference >= start_win_ts) 
         ||
          element->get_first_timestamp() + (element->get_num_frames() - 1) * RDT::expected_tick_difference >=
            end_win_ts) {
        // We don't need the whole aggregated object (e.g.: superchunk)
        for (auto frame_iter = element->begin(); frame_iter != element->end(); frame_iter++) {
          if (get_frame_iterator_timestamp(frame_iter) >= start_win_ts &&
              get_frame_iterator_timestamp(frame_iter) < end_win_ts) {
            frag_pieces.emplace_back(
              std::make_pair<void*, size_t>(static_cast<void*>(&(*frame_iter)), element->get_frame_size()));
          }
        }
      } else {
        // We are somewhere in the middle -> the whole aggregated object (e.g.: superchunk) can be copied
        frag_pieces.emplace_back(
          std::make_pair<void*, size_t>(static_cast<void*>((*start_iter).begin()), element->get_payload_size()));
      }

      elements_handled++;
      ++start_iter;
      element = &(*start_iter);
    }
  }
  return frag_pieces;
}

template<class RDT, class LBT>
typename DefaultRequestHandlerModel<RDT, LBT>::RequestResult 
DefaultRequestHandlerModel<RDT, LBT>::data_request(dfmessages::DataRequest dr, 
                                                   bool send_partial_fragment_if_available)
{
  // Prepare response
  RequestResult rres(ResultCode::kUnknown, dr);

  // Prepare FragmentHeader and empty Fragment pieces list
  auto frag_header = create_fragment_header(dr);
  std::vector<std::pair<void*, size_t>> frag_pieces;
  std::ostringstream oss;

  bool local_data_not_found_flag = false;
  if (m_latency_buffer->occupancy() != 0) {
    // Data availability is calculated here
    auto front_element = m_latency_buffer->front();           // NOLINT
    auto last_element = m_latency_buffer->back();             // NOLINT
    uint64_t last_ts = front_element->get_first_timestamp();  // NOLINT(build/unsigned)
    uint64_t newest_ts = last_element->get_first_timestamp(); // NOLINT(build/unsigned)

    uint64_t start_win_ts = dr.request_information.window_begin; // NOLINT(build/unsigned)
    uint64_t end_win_ts = dr.request_information.window_end;     // NOLINT(build/unsigned)
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Data request for trig/seq_num=" << dr.trigger_number
      << "." << dr.sequence_number << " with"
      << " Trigger TS=" << dr.trigger_timestamp
      << " Oldest stored TS=" << last_ts
      << " Newest stored TS=" << newest_ts
      << " Start of window TS=" << start_win_ts
      << " End of window TS=" << end_win_ts
      << " Latency buffer occupancy=" << m_latency_buffer->occupancy();

    // List of safe-extraction conditions
    if (last_ts <= start_win_ts && end_win_ts <= newest_ts) { // the full window of data is there
      frag_pieces = get_fragment_pieces(start_win_ts, end_win_ts, rres);
    } else if (send_partial_fragment_if_available && last_ts <= end_win_ts && end_win_ts <= newest_ts) { // partial data is there
      frag_pieces = get_fragment_pieces(start_win_ts, end_win_ts, rres);
      if (rres.result_code == ResultCode::kNotYet) {
        // 15-Sep-2022, KAB: this is really ugly.  I'm not sure why get_fragment_pieces occasionally
        // returns kNotYet when running with long readout windows, but I suspect that it has something
        // to do with that code assuming that the readout window is fully contained within the data
        // that exists in the latency buffer.  In any case, this code simply bails out when that happens.
        local_data_not_found_flag = true;
        rres.result_code = ResultCode::kNotFound;
      }
    } else if ((! send_partial_fragment_if_available) && last_ts > start_win_ts) { // data at the start of the window is missing
      frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
      rres.result_code = ResultCode::kNotFound;
      ++m_num_requests_old_window;
      ++m_num_requests_bad;
    } else if (send_partial_fragment_if_available && last_ts > end_win_ts) { // data is completely gone
      local_data_not_found_flag = true;
      frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
      rres.result_code = ResultCode::kNotFound;
      ++m_num_requests_old_window;
      ++m_num_requests_bad;
    } else if (newest_ts < end_win_ts) { // data at the end of the window is missing (more could still arrive)
      if (send_partial_fragment_if_available) {
        // We've been asked to send the partial fragment if we don't
        // have an object past the end of the window, so fill the
        // fragment with what we have so far
        if (m_warn_on_timeout) {
          TLOG()
            << "Returning partial fragment for trig/seq number " << dr.trigger_number << "." << dr.sequence_number
            << " with TS " << dr.trigger_timestamp 
            << ". Component " << dr.request_information.component 
            << " with type " << daqdataformats::fragment_type_to_string(daqdataformats::FragmentType(frag_header.fragment_type));
        } else {
          TLOG_DEBUG(TLVL_WORK_STEPS)
            << "Returning partial fragment for trig/seq number " << dr.trigger_number << "." << dr.sequence_number
            << " with TS " << dr.trigger_timestamp 
            << ". Component " << dr.request_information.component 
            << " with type " << daqdataformats::fragment_type_to_string(daqdataformats::FragmentType(frag_header.fragment_type));
        }
        frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kIncomplete));
        frag_pieces = get_fragment_pieces(start_win_ts, end_win_ts, rres);
        // 06-Jul-2022, KAB: added the following line to translate a kNotYet status code from
        // get_fragment_pieces() to kFound. The reasoning behind this addition is that when
        // send_partial_fragment_if_available is set to true, we should accept whatever we've got
        // in the buffer and *not* retry any longer. So, we force that behavior with the
        // following line. The data-taking scenario which illustrates
        // this situation is long-window-readout in which a given trigger is split into a sequence
        // of TriggerRecords and the 2..Nth DataRequest find nothing useful in the TriggerCandidate
        // (TCBuffer) latency buffer (and no data later than the request window end time).
        // In those cases, get_fragment_pieces() returns kNotYet, and we need to change that
        // kFound in order to not keep looping.
        if (rres.result_code == ResultCode::kNotYet) {rres.result_code = ResultCode::kFound;}
      } else {
        ++m_num_requests_delayed;
        rres.result_code = ResultCode::kNotYet; // give it another chance
      }
    } else {
      TLOG() << "Don't know how to categorise this request";
      frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
      rres.result_code = ResultCode::kNotFound;
      ++m_num_requests_uncategorized;
      ++m_num_requests_bad;
    }

    // Requeue if needed
    if (rres.result_code == ResultCode::kNotYet) {
      if (m_run_marker.load()) {
        return rres; // If kNotYet, return immediately, don't check for fragment pieces.
      } else {
        frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
        rres.result_code = ResultCode::kNotFound;
        ++m_num_requests_bad;
      }
    }

    // Build fragment
    oss << "TS match result on link " << m_sourceid.id << ": "
        << " Trigger number=" << dr.trigger_number
        << " Oldest stored TS=" << last_ts
        << " Start of window TS=" << start_win_ts
        << " End of window TS=" << end_win_ts
        << " Estimated newest stored TS=" << newest_ts
        << " Requestor=" << dr.data_destination;
    TLOG_DEBUG(TLVL_WORK_STEPS) << oss.str();
  } else {
    local_data_not_found_flag = true;
    if (m_warn_about_empty_buffer) {
      ers::warning(RequestOnEmptyBuffer(ERS_HERE, m_sourceid, "Data not found"));
    } else {
      TLOG_DEBUG(TLVL_WORK_STEPS) << "SourceID[" << m_sourceid << "] Request on empty buffer: Data not found";
    }
    frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
    rres.result_code = ResultCode::kNotFound;
    ++m_num_requests_bad;
  }

  if (rres.result_code != ResultCode::kFound) {
    if (m_warn_about_empty_buffer || (! local_data_not_found_flag)) {
      ers::warning(dunedaq::readoutlibs::TrmWithEmptyFragment(ERS_HERE, m_sourceid, oss.str()));
    } else {
      TLOG_DEBUG(TLVL_WORK_STEPS) << "SourceID[" << m_sourceid << "] Trigger Matching result with empty fragment: " << oss.str();
    }
  }

  // Create fragment from pieces
  rres.fragment = std::make_unique<daqdataformats::Fragment>(frag_pieces);

  // Set header
  rres.fragment->set_header_fields(frag_header);

  return rres;
}

} // namespace readoutlibs
} // namespace dunedaq
