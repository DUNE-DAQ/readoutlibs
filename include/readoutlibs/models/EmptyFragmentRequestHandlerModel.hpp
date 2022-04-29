/**
 * @file EmptyFragmentRequestHandlerModel.hpp Request handler that always returns empty fragments
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_EMPTYFRAGMENTREQUESTHANDLERMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_EMPTYFRAGMENTREQUESTHANDLERMODEL_HPP_

#include "readoutlibs/models/DefaultRequestHandlerModel.hpp"

#include <memory>
#include <utility>
#include <vector>

using dunedaq::readoutlibs::logging::TLVL_HOUSEKEEPING;
using dunedaq::readoutlibs::logging::TLVL_QUEUE_PUSH;
using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType, class LatencyBufferType>
class EmptyFragmentRequestHandlerModel : public DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>
{
public:
  using inherited = DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>;

  explicit EmptyFragmentRequestHandlerModel(std::unique_ptr<LatencyBufferType>& latency_buffer,
                                            std::unique_ptr<FrameErrorRegistry>& error_registry)
    : DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>(latency_buffer, error_registry)
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "EmptyFragmentRequestHandlerModel created...";
  }

  using RequestResult =
    typename dunedaq::readoutlibs::RequestHandlerConcept<ReadoutType, LatencyBufferType>::RequestResult;
  using ResultCode = typename dunedaq::readoutlibs::RequestHandlerConcept<ReadoutType, LatencyBufferType>::ResultCode;

  void issue_request(dfmessages::DataRequest datarequest,
                     iomanager::SenderConcept<std::pair<std::unique_ptr<daqdataformats::Fragment>, std::string>>& fragment_sender) override
  {
    auto frag_header = inherited::create_fragment_header(datarequest);
    frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
    auto fragment = std::make_unique<daqdataformats::Fragment>(std::vector<std::pair<void*, size_t>>());
    fragment->set_header_fields(frag_header);

    // ers::warning(dunedaq::readoutlibs::TrmWithEmptyFragment(ERS_HERE, "DLH is configured to send empty fragment"));
    TLOG_DEBUG(TLVL_WORK_STEPS) << "DLH is configured to send empty fragment";

    try { // Push to Fragment queue
      TLOG_DEBUG(TLVL_QUEUE_PUSH) << "Sending fragment with trigger_number " << fragment->get_trigger_number()
                                  << ", run number " << fragment->get_run_number() << ", and GeoID "
                                  << fragment->get_element_id();
      auto frag = std::make_pair(std::move(fragment), datarequest.data_destination);
      fragment_sender.send(frag, std::chrono::milliseconds(inherited::m_fragment_queue_timeout));
    } catch (const ers::Issue& excpt) {
      ers::warning(CannotWriteToQueue(
        ERS_HERE, DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>::m_geoid, "fragment queue"));
    }
  }
};

} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_EMPTYFRAGMENTREQUESTHANDLERMODEL_HPP_
