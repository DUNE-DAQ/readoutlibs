// Declarations for DefaultSkipListRequestHandler

namespace dunedaq {
namespace readoutlibs {

template<class T>
void 
DefaultSkipListRequestHandler<T>::skip_list_cleanup_request()
{
  // size_t occupancy_guess = m_latency_buffer->occupancy();
  size_t removed_ctr = 0;
  uint64_t tailts = 0; // oldest // NOLINT(build/unsigned)
  uint64_t headts = 0; // newest // NOLINT(build/unsigned)
  {
    SkipListAcc acc(inherited::m_latency_buffer->get_skip_list());
    auto tail = acc.last();
    auto head = acc.first();
    if (tail && head) {
      // auto tailptr = reinterpret_cast<const fddetdataformats::DAPHNEFrame*>(tail); // NOLINT
      // auto headptr = reinterpret_cast<const fddetdataformats::DAPHNEFrame*>(head); // NOLINT
      tailts = (*tail).get_first_timestamp(); // tailptr->get_timestamp();
      headts = (*head).get_first_timestamp(); // headptr->get_timestamp();
      TLOG_DEBUG(TLVL_WORK_STEPS) << "Cleanup REQUEST with "
                                  << "Oldest stored TS=" << headts << " "
                                  << "Newest stored TS=" << tailts;
      if (tailts - headts > m_max_ts_diff) { // ts differnce exceeds maximum
        ++(inherited::m_pop_reqs);
        uint64_t timediff = m_max_ts_diff; // NOLINT(build/unsigned)
        while (timediff >= m_max_ts_diff) {
          bool removed = acc.remove(*head);
          if (!removed) {
            TLOG_DEBUG(TLVL_WORK_STEPS) << "Unsuccesfull remove from SKL during cleanup: " << removed;
          } else {
            ++removed_ctr;
          }
          head = acc.first();
          // headptr = reinterpret_cast<const fddetdataformats::DAPHNEFrame*>(head);
          headts = (*head).get_first_timestamp(); // headptr->get_timestamp();
          timediff = tailts - headts;
        }
        inherited::m_pops_count += removed_ctr;
      }
    } else {
      TLOG_DEBUG(TLVL_WORK_STEPS) << "Didn't manage to get SKL head and tail!";
    }
  }
  inherited::m_num_buffer_cleanups++;
}

} // namespace readoutlibs
} // namespace dunedaq
