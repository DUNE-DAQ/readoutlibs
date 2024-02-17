/**
 * @file DataSubscriberModel.hpp
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_SRC_SOURCEMODEL_HPP_
#define READOUTLIBS_SRC_SOURCEMODEL_HPP_

#include "readoutlibs/concepts/SourceConcept.hpp"


#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/Receiver.hpp"
#include "logging/Logging.hpp"
//#include "readoutlibs/utils/ReusableThread.hpp"

#include "coredal/Connection.hpp"

//#include <folly/ProducerConsumerQueue.h>
//#include <nlohmann/json.hpp>

//#include <atomic>
//#include <memory>
//#include <mutex>
#include <functional>

namespace dunedaq::readoutlibs {

template<class OriginalPayloadType, class TargetPayloadType>
class DataSubscriberModel : public SourceConcept
{
public: 
  using inherited = SourceConcept;

  /**
   * @brief SourceModel Constructor
   * @param name Instance name for this SourceModel instance
   */
  DataSubscriberModel(): SourceConcept() {}
  ~DataSubscriberModel() {}

  void init(const coredal::DaqModule* cfg) override {
    if (cfg->get_outputs().size() != 1) {
      throw readoutlibs::InitializationError(ERS_HERE, "Only 1 output supported for subscribers");
    }
    m_data_sender = get_iom_sender<TargetPayloadType>(cfg->get_outputs()[0]->UID());

    if (cfg->get_inputs().size() != 1) {
      throw readoutlibs::InitializationError(ERS_HERE, "Only 1 input supported for subscribers");
    }
    m_data_receiver = get_iom_receiver<OriginalPayloadType>(cfg->get_inputs()[0]->UID());
  }

  void start() {
    m_data_receiver->add_callback(std::bind(&DataSubscriberModel::handle_payload, this, std::placeholders::_1));
  }  

  void stop() {
    m_data_receiver->remove_callback();
  }

  void get_info(opmonlib::InfoCollector& /*ci*/, int /*level*/) 
  {
  // FIXME: implement statistics publishing
  }

  bool handle_payload(OriginalPayloadType&  message) // NOLINT(build/unsigned)
  {
    TargetPayloadType& target_payload = reinterpret_cast<TargetPayloadType&>(message);
    if (!m_data_sender->try_send(std::move(target_payload), iomanager::Sender::s_no_block)) {
      ++m_dropped_packets;
    }
    return true;
  }

private:
  using source_t = dunedaq::iomanager::ReceiverConcept<OriginalPayloadType>;
  std::shared_ptr<source_t> m_data_receiver;

  using sink_t = dunedaq::iomanager::SenderConcept<TargetPayloadType>;
  std::shared_ptr<sink_t> m_data_sender;

  std::atomic<uint64_t> m_dropped_packets{0};
};

} // namespace dunedaq::readoutlibs

#endif // READOUTLIBS_SRC_SOURCEMODEL_HPP_
