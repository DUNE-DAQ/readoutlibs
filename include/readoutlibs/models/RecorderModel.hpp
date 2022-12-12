/**
 * @file RecorderModel.hpp Templated recorder implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_RECORDERMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_RECORDERMODEL_HPP_

#include "appfwk/DAQModuleHelper.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Receiver.hpp"
#include "readoutlibs/ReadoutTypes.hpp"
#include "readoutlibs/concepts/RecorderConcept.hpp"
#include "readoutlibs/recorderconfig/Nljs.hpp"
#include "readoutlibs/recorderconfig/Structs.hpp"
#include "readoutlibs/recorderinfo/InfoStructs.hpp"
#include "readoutlibs/utils/BufferedFileWriter.hpp"
#include "readoutlibs/utils/ReusableThread.hpp"
#include "utilities/WorkerThread.hpp"

#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
class RecorderModel : public RecorderConcept
{
public:
  explicit RecorderModel(std::string name)
    : m_work_thread(0)
    , m_name(name)
  {
  }

  void init(const nlohmann::json& args) override;
  void get_info(opmonlib::InfoCollector& ci, int /* level */) override;
  void do_conf(const nlohmann::json& args) override;
  void do_scrap(const nlohmann::json& /*args*/) override { m_buffered_writer.close(); }
  void do_start(const nlohmann::json& /* args */) override;
  void do_stop(const nlohmann::json& /* args */) override;

private:
  // The work that the worker thread does
  void do_work();

  // Queue
  using source_t = dunedaq::iomanager::ReceiverConcept<ReadoutType>;
  std::shared_ptr<source_t> m_data_receiver;

  // Internal
  recorderconfig::Conf m_conf;
  BufferedFileWriter<> m_buffered_writer;

  // Threading
  ReusableThread m_work_thread;
  std::atomic<bool> m_run_marker;

  // Stats
  std::atomic<int> m_packets_processed_total{ 0 };
  std::atomic<int> m_packets_processed_since_last_info{ 0 };
  std::chrono::steady_clock::time_point m_time_point_last_info;

  std::string m_name;
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/RecorderModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_RECORDERMODEL_HPP_
