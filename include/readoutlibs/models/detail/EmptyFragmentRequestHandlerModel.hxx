// Declarations for EmptyFragmentRequestHandlerModel

namespace dunedaq {
namespace readoutlibs {

// Override the issue_request implementation of the DefaultRequestHandlerModel
// in order to always respond with empty fragments.
template<class ReadoutType, class LatencyBufferType>
void 
EmptyFragmentRequestHandlerModel<ReadoutType, LatencyBufferType>::issue_request(
  dfmessages::DataRequest datarequest,
  bool /*send_partial_fragment_if_not_yet*/)
{
  auto frag_header = inherited::create_fragment_header(datarequest);
  frag_header.error_bits |= (0x1 << static_cast<size_t>(daqdataformats::FragmentErrorBits::kDataNotFound));
  auto fragment = std::make_unique<daqdataformats::Fragment>(std::vector<std::pair<void*, size_t>>());
  fragment->set_header_fields(frag_header);

  // ers::warning(dunedaq::readoutlibs::TrmWithEmptyFragment(ERS_HERE, "DLH is configured to send empty fragment"));
  TLOG_DEBUG(TLVL_WORK_STEPS) << "DLH is configured to send empty fragment";

  try { // Push to Fragment queue
    TLOG_DEBUG(TLVL_QUEUE_PUSH) << "Sending fragment with trigger_number " << fragment->get_trigger_number()
                                << ", run number " << fragment->get_run_number() << ", and SourceID "
                                << fragment->get_element_id();
    //auto frag = std::make_pair(std::move(fragment), datarequest.data_destination);
    get_iom_sender<std::unique_ptr<daqdataformats::Fragment>>(datarequest.data_destination)
      ->send(std::move(fragment), std::chrono::milliseconds(10));
  } catch (const ers::Issue& excpt) {
    ers::warning(CannotWriteToQueue(
      ERS_HERE, DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>::m_sourceid, "fragment queue", excpt));
  }
}

} // namespace readoutlibs
} // namespace dunedaq
