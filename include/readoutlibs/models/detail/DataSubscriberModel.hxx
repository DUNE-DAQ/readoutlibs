// Declarations for DataSubscriberModel

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
void 
DataSubscriberModel<ReadoutType>::init(const appdal::DataSubscriber* conf)
{
  for (auto input : conf->get_inputs()) {
    try {
      m_data_receiver = get_iom_receiver<ReadoutType>(input->UID());
    } catch (const ers::Issue& excpt) {
      throw ResourceQueueError(ERS_HERE, m_name, "DataSubscriberModel");
    }
  }
  for (auto output : conf->get_outputs()) {
    try {
      m_data_sender = get_iom_sender<ReadoutType>(output->UID());
    } catch (const ers::Issue& excpt) {
      throw ResourceQueueError(ERS_HERE, m_name, "DataSubscriberModel");
    }
  }
}

template<class ReadoutType>
void 
DataSubscriberModel<ReadoutType>::get_info(opmonlib::InfoCollector& /*ci*/, int /* level */)
{
 
}


template<class ReadoutType>
void 
DataSubscriberModel<ReadoutType>::do_start(const nlohmann::json& /* args */)
{
    m_data_receiver->add_callback(std::bind(&DataSubscriberModel::dispatch, this, std::placeholders::_1))
}

template<class ReadoutType>
void 
DataSubscriberModel<ReadoutType>::do_stop(const nlohmann::json& /* args */)
{
  m_data_receiver->remove_callback();
}

template<class ReadoutType>
void 
DataSubscriberModel<ReadoutType>::dispatch(ReadoutType& data)
{  
  if (!m_data_sender->try_send(std::move(data), iomanager::Sender::s_no_block)) {
    ers::warning(CannotDispatch(ERS_HERE, m_name));
  }
}

} // namespace readoutlibs
} // namespace dunedaq
