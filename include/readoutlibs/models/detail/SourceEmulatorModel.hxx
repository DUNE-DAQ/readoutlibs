//Declarations for SourceEmulatorModel

using dunedaq::readoutlibs::logging::TLVL_TAKE_NOTE;
using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
void 
SourceEmulatorModel<ReadoutType>::set_sender(const std::string& conn_name)
{
  if (!m_sender_is_set) {
    m_raw_data_sender = get_iom_sender<ReadoutType>(conn_name);
    m_sender_is_set = true;
  } else {
    // ers::error();
  }
}

template<class ReadoutType>
void 
SourceEmulatorModel<ReadoutType>::conf(const nlohmann::json& args, 
                                       const nlohmann::json& link_conf)
{
  if (m_is_configured) {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "This emulator is already configured!";
  } else {
    m_conf = args.get<module_conf_t>();
    m_link_conf = link_conf.get<link_conf_t>();
    m_raw_sender_timeout_ms = std::chrono::milliseconds(m_conf.queue_timeout_ms);

    std::mt19937 mt(rand()); // NOLINT(runtime/threadsafe_fn)
    std::uniform_real_distribution<double> dis(0.0, 1.0);

    m_geoid.element_id = m_link_conf.geoid.element;
    m_geoid.region_id = m_link_conf.geoid.region;
    m_geoid.system_type = ReadoutType::system_type;

    m_file_source = std::make_unique<FileSourceBuffer>(m_link_conf.input_limit, sizeof(ReadoutType));
    try {
      m_file_source->read(m_link_conf.data_filename);
    } catch (const ers::Issue& ex) {
      ers::fatal(ex);
      throw ConfigurationError(ERS_HERE, m_geoid, "", ex);
    }
    m_dropouts_length = m_link_conf.random_population_size;
    if (m_dropout_rate == 0.0) {
      m_dropouts = std::vector<bool>(1);
    } else {
      m_dropouts = std::vector<bool>(m_dropouts_length);
    }
    for (size_t i = 0; i < m_dropouts.size(); ++i) {
      m_dropouts[i] = dis(mt) >= m_dropout_rate;
    }

    m_frame_errors_length = m_link_conf.random_population_size;
    m_frame_error_rate = m_link_conf.emu_frame_error_rate;
    m_error_bit_generator = ErrorBitGenerator(m_frame_error_rate);
    m_error_bit_generator.generate();

    m_is_configured = true;
  }
  // Configure thread:
  m_producer_thread.set_name("fakeprod", m_link_conf.geoid.element);
} 

template<class ReadoutType>
void 
SourceEmulatorModel<ReadoutType>::start(const nlohmann::json& /*args*/)
{
  m_packet_count_tot = 0;
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Starting threads...";
  m_rate_limiter = std::make_unique<RateLimiter>(m_rate_khz / m_link_conf.slowdown);
  // m_stats_thread.set_work(&SourceEmulatorModel<ReadoutType>::run_stats, this);
  m_producer_thread.set_work(&SourceEmulatorModel<ReadoutType>::run_produce, this);
}

template<class ReadoutType>
void 
SourceEmulatorModel<ReadoutType>::stop(const nlohmann::json& /*args*/)
{
  while (!m_producer_thread.get_readiness()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

template<class ReadoutType>
void 
SourceEmulatorModel<ReadoutType>::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  sourceemulatorinfo::Info info;
  info.packets = m_packet_count_tot.load();
  info.new_packets = m_packet_count.exchange(0);

  ci.add(info);
}

template<class ReadoutType>
void 
SourceEmulatorModel<ReadoutType>::run_produce()
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Data generation thread " << m_this_link_number << " started";

  // pthread_setname_np(pthread_self(), get_name().c_str());

  uint offset = 0; // NOLINT(build/unsigned)
  auto& source = m_file_source->get();

  int num_elem = m_file_source->num_elements();
  if (num_elem == 0) {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "No elements to read from buffer! Sleeping...";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    num_elem = m_file_source->num_elements();
  }

  auto rptr = reinterpret_cast<ReadoutType*>(source.data()); // NOLINT

  // set the initial timestamp to a configured value, otherwise just use the timestamp from the header
  uint64_t ts_0 = (m_conf.set_t0_to >= 0) ? m_conf.set_t0_to : rptr->get_first_timestamp(); // NOLINT(build/unsigned)
  TLOG_DEBUG(TLVL_BOOKKEEPING) << "First timestamp in the source file: " << ts_0;
  uint64_t timestamp = ts_0; // NOLINT(build/unsigned)
  int dropout_index = 0;

  while (m_run_marker.load()) {
    // Which element to push to the buffer
    if (offset == num_elem * sizeof(ReadoutType) || (offset + 1) * sizeof(ReadoutType) > source.size()) {
      offset = 0;
    }

    bool create_frame = m_dropouts[dropout_index]; // NOLINT(runtime/threadsafe_fn)
    dropout_index = (dropout_index + 1) % m_dropouts.size();
    if (create_frame) {
      ReadoutType payload;
      // Memcpy from file buffer to flat char array
      ::memcpy(static_cast<void*>(&payload),
               static_cast<void*>(source.data() + offset * sizeof(ReadoutType)),
               sizeof(ReadoutType));

      // Fake timestamp
      payload.fake_timestamps(timestamp, m_time_tick_diff);

      // Introducing frame errors
      std::vector<uint16_t> frame_errs; // NOLINT(build/unsigned)
      for (size_t i = 0; i < rptr->get_num_frames(); ++i) {
        frame_errs.push_back(m_error_bit_generator.next());
      }
      payload.fake_frame_errors(&frame_errs);

      // send it
      try {
        m_raw_data_sender->send(std::move(payload), m_raw_sender_timeout_ms);
      } catch (ers::Issue& excpt) {
        ers::warning(CannotWriteToQueue(ERS_HERE, m_geoid, "raw data input queue", excpt));
        // std::runtime_error("Queue timed out...");
      }

      // Count packet and limit rate if needed.
      ++offset;
      ++m_packet_count;
      ++m_packet_count_tot;
    }

    timestamp += m_time_tick_diff * 12;

    m_rate_limiter->limit();
  }
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Data generation thread " << m_this_link_number << " finished";
}

} // namespace readoutlibs
} // namespace dunedaq
