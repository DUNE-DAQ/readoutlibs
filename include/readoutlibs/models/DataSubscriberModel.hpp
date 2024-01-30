/**
 * @file DataSubscriberModel.hpp Templated recorder implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_DATASUBSCRIBERMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_DATASUBSCRIBERMODEL_HPP_

#include "iomanager/IOManager.hpp"
#include "iomanager/Receiver.hpp"
#include "readoutlibs/ReadoutTypes.hpp"
#include "readoutlibs/concepts/DataSubscriberConcept.hpp"
#include "readoutlibs/utils/ReusableThread.hpp"

#include "coredal/DaqModule.hpp"
#include "coredal/Connection.hpp"
#include "appdal/DataReaderConf.hpp"

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
class DataSubscriberModel : public DataSubscriberConcept
{
public:
  explicit DataSubscriberModel(std::string name)
    : m_name(name)
  {}

  void init(const appdal::DataSubscriber* conf) override;
  void get_info(opmonlib::InfoCollector& /*ci*/, int /* level */) override;
  void do_start(const nlohmann::json& /* args */) override;
  void do_stop(const nlohmann::json& /* args */) override;
  void dispatch() override;
private:
  std::string m_name;
  // Subscription
  using source_t = dunedaq::iomanager::ReceiverConcept<ReadoutType>;
  std::shared_ptr<source_t> m_data_receiver;

  // Data forwarding
  using sink_t = iomanager::SenderConcept<ReadoutType>;
  std::shared_ptr<sink_t> m_data_sender;

  // Stats
  //std::atomic<int> m_packets_processed_total{ 0 };
  //std::atomic<int> m_packets_processed_since_last_info{ 0 };
  //std::chrono::steady_clock::time_point m_time_point_last_info;
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/DataSubscriberModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_DATASUBSCRIBERMODEL_HPP_