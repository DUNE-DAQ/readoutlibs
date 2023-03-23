// Declarations for FixedRateQueueModel

namespace dunedaq {
namespace readoutlibs {

template<typename T>
typename IterableQueueModel<T>::Iterator 
FixedRateQueueModel<T>::lower_bound(T& element, bool with_errors)
{
  if (with_errors) {
    return BinarySearchQueueModel<T>::lower_bound(element, with_errors);
  }
  uint64_t timestamp = element.get_first_timestamp(); // NOLINT(build/unsigned)
  unsigned int start_index =
    IterableQueueModel<T>::readIndex_.load(std::memory_order_relaxed); // NOLINT(build/unsigned)
  size_t occupancy_guess = IterableQueueModel<T>::occupancy();
  uint64_t last_ts = IterableQueueModel<T>::records_[start_index].get_first_timestamp(); // NOLINT(build/unsigned)
  uint64_t newest_ts =                                                                   // NOLINT(build/unsigned)
    last_ts +
    occupancy_guess * T::expected_tick_difference * IterableQueueModel<T>::records_[start_index].get_num_frames();
  size_t n_frames = IterableQueueModel<T>::records_[start_index].get_num_frames();
  
  if (last_ts > timestamp || timestamp > newest_ts) {
    return IterableQueueModel<T>::end();
  }

  int64_t time_tick_diff = (timestamp - last_ts) / T::expected_tick_difference;

  //if we are aligned on a n_frame boundary, 
  uint32_t num_element_offset = time_tick_diff/n_frames; // NOLINT(build/unsigned)
  uint32_t target_index = start_index + num_element_offset; // NOLINT(build/unsigned)

  //if we aren't perfectly aligned on a n_frames boundary, move us up so we satisfy normal lower_bound rules
  if(time_tick_diff%n_frames!=0) ++target_index;
  
  if (target_index >= IterableQueueModel<T>::size_) {
    target_index -= IterableQueueModel<T>::size_;
  }

  return typename IterableQueueModel<T>::Iterator(*this, target_index);
}

} // namespace readoutlibs
} // namespace dunedaq
