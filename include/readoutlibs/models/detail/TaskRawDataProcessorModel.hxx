// Declarations for TaskRawDataProcessorModel

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::conf(const appdal::ReadoutModule* conf)
{
  //auto config = cfg["rawdataprocessorconf"].get<readoutconfig::RawDataProcessorConf>();
  //m_emulator_mode = config.emulator_mode;
  auto cfg = conf->get_data_processor();
  m_postprocess_queue_sizes = cfg->get_queue_sizes();

  for (size_t i = 0; i < m_post_process_functions.size(); ++i) {
    m_items_to_postprocess_queues.push_back(
      std::make_unique<folly::ProducerConsumerQueue<const ReadoutType*>>(m_postprocess_queue_sizes));
    m_post_process_threads[i]->set_name(cfg->get_thread_names_prefix() + std::to_string(i), sid.id);
  }
  m_sourceid.ud = conf->get_source_id();
  m_sourceid.subsystem = ReadoutType::subsystem;
}

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::scrap(const nlohmann::json& /*cfg*/)
{
  m_items_to_postprocess_queues.clear();
  m_post_process_threads.clear();
  m_post_process_functions.clear();
  m_preprocess_functions.clear();
}

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::start(const nlohmann::json& /*args*/)
{
  // m_last_processed_daq_ts =
  // std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  m_run_marker.store(true);
  for (size_t i = 0; i < m_post_process_threads.size(); ++i) {
    m_post_process_threads[i]->set_work(&TaskRawDataProcessorModel<ReadoutType>::run_post_processing_thread,
                                        this,
                                        std::ref(m_post_process_functions[i]),
                                        std::ref(*m_items_to_postprocess_queues[i]));
  }
}

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::stop(const nlohmann::json& /*args*/)
{
  m_run_marker.store(false);
  for (auto& thread : m_post_process_threads) {
    while (!thread->get_readiness()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::postprocess_item(const ReadoutType* item)
{
  for (size_t i = 0; i < m_items_to_postprocess_queues.size(); ++i) {
    if (!m_items_to_postprocess_queues[i]->write(item)) {
      ers::warning(PostprocessingNotKeepingUp(ERS_HERE, m_sourceid, i));
    }
  }
}

template<class ReadoutType>
template<typename Task>
void 
TaskRawDataProcessorModel<ReadoutType>::add_preprocess_task(Task&& task)
{
  m_preprocess_functions.push_back(std::forward<Task>(task));
}

template<class ReadoutType>
template<typename Task>
void 
TaskRawDataProcessorModel<ReadoutType>::add_postprocess_task(Task&& task)
{
  m_post_process_threads.emplace_back(std::make_unique<ReusableThread>(0));
  m_post_process_functions.push_back(std::forward<Task>(task));
}

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::invoke_all_preprocess_functions(ReadoutType* item)
{
  for (auto&& task : m_preprocess_functions) {
    task(item);
  }
}

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::launch_all_preprocess_functions(ReadoutType* item)
{
  for (auto&& task : m_preprocess_functions) {
    auto fut = std::async(std::launch::async, task, item);
  }
}

template<class ReadoutType>
void 
TaskRawDataProcessorModel<ReadoutType>::run_post_processing_thread(
  std::function<void(const ReadoutType*)>& function,
  folly::ProducerConsumerQueue<const ReadoutType*>& queue)
{
  while (m_run_marker.load() || queue.sizeGuess() > 0) {
    const ReadoutType* element;
    if (queue.read(element)) {
      function(element);
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  }
}

} // namespace readoutlibs
} // namespace dunedaq
