// Declarations for RecorderModel

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
void 
RecorderModel<ReadoutType>::init(const appdal::DataRecorder* conf)
{
  for (input : conf->get_inputs()) {
    try {
      m_data_receiver = get_iom_receiver<ReadoutType>(input->UID());
    } catch (const ers::Issue& excpt) {
      throw ResourceQueueError(ERS_HERE, "raw_recording", "RecorderModel");
    }
  }
  m_output_file = conf->get_configuration()->get_output_file();
  m_stream_buffer_size = conf->get_configuration()->get_streaming_buffer_size();
  m_compression_algorithm =  conf->get_configuration()->get_compression_algorithm();
  m_use_o_direct = conf->get_configuration()->get_use_o_direct();
}

template<class ReadoutType>
void 
RecorderModel<ReadoutType>::get_info(opmonlib::InfoCollector& ci, int /* level */)
{
  recorderinfo::Info info;
  info.packets_processed = m_packets_processed_total;
  double time_diff = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() -
                                                                               m_time_point_last_info)
                       .count();
  info.throughput_processed_packets = m_packets_processed_since_last_info / time_diff;

  ci.add(info);

  m_packets_processed_since_last_info = 0;
  m_time_point_last_info = std::chrono::steady_clock::now();
}

template<class ReadoutType>
void 
RecorderModel<ReadoutType>::do_conf(const nlohmann::json& args)
{
  
  if (remove(m_output_file.c_str()) == 0) {
    TLOG(TLVL_WORK_STEPS) << "Removed existing output file from previous run" << std::endl;
  }

  m_buffered_writer.open(
    m_output_file, m_stream_buffer_size, m_compression_algorithm, m_use_o_direct);
  m_work_thread.set_name(get_name(), 0);
}

template<class ReadoutType>
void 
RecorderModel<ReadoutType>::do_start(const nlohmann::json& /* args */)
{
  m_packets_processed_total = 0;
  m_run_marker.store(true);
  m_work_thread.set_work(&RecorderModel<ReadoutType>::do_work, this);
}

template<class ReadoutType>
void 
RecorderModel<ReadoutType>::do_stop(const nlohmann::json& /* args */)
{
  m_run_marker.store(false);
  while (!m_work_thread.get_readiness()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

template<class ReadoutType>
void 
RecorderModel<ReadoutType>::do_work()
{
  m_time_point_last_info = std::chrono::steady_clock::now();

  ReadoutType element;
  while (m_run_marker) {
    try {
      element = m_data_receiver->receive(std::chrono::milliseconds(100)); // RS -> Use confed timeout?
      m_packets_processed_total++;
      m_packets_processed_since_last_info++;
      if (!m_buffered_writer.write(reinterpret_cast<char*>(&element), sizeof(element))) { // NOLINT
        ers::warning(CannotWriteToFile(ERS_HERE, m_conf.output_file));
        break;
      }
    } catch (const dunedaq::iomanager::TimeoutExpired& excpt) {
      continue;
    }
  }
  m_buffered_writer.flush();
}

} // namespace readoutlibs
} // namespace dunedaq
