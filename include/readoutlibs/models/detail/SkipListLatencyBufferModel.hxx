// Declarations for SkipListLatencyBufferModel

namespace dunedaq {
namespace readoutlibs {

template<class T>
size_t
SkipListLatencyBufferModel<T>::occupancy() const
{
  auto occupancy = 0;
  {
    SkipListTAcc acc(m_skip_list);
    occupancy = acc.size();
  }
  return occupancy;
}

template<class T>
bool
SkipListLatencyBufferModel<T>::write(T&& new_element)
{
  bool success = false;
  {
    SkipListTAcc acc(m_skip_list);
    auto ret = acc.insert(std::move(new_element)); // ret T = std::pair<iterator, bool>
    success = ret.second;
  }
  return success;
}

template<class T>
bool
SkipListLatencyBufferModel<T>::put(T& new_element)
{
  bool success = false;
  {
    SkipListTAcc acc(m_skip_list);
    auto ret = acc.insert(new_element); // ret T = std::pair<iterator, bool>
    success = ret.second;
  }
  return success;
}

template<class T>
bool
SkipListLatencyBufferModel<T>::read(T& element)
{
  bool found = false;
  {
    SkipListTAcc acc(m_skip_list);
    auto lb_node = acc.begin();
    found = (lb_node == acc.end()) ? false : true;
    if (found) {
      element = *lb_node;
    }
  }
  return found;
}

template<class T>
typename SkipListLatencyBufferModel<T>::Iterator
SkipListLatencyBufferModel<T>::begin()
{
  SkipListTAcc acc = SkipListTAcc(m_skip_list);
  SkipListTIter iter = acc.begin();
  return std::move(SkipListLatencyBufferModel<T>::Iterator(std::move(acc), iter));
}

template<class T>
typename SkipListLatencyBufferModel<T>::Iterator
SkipListLatencyBufferModel<T>::end()
{
  SkipListTAcc acc = SkipListTAcc(m_skip_list);
  SkipListTIter iter = acc.end();
  return std::move(SkipListLatencyBufferModel<T>::Iterator(std::move(acc), iter));
}

template<class T>
typename SkipListLatencyBufferModel<T>::Iterator
SkipListLatencyBufferModel<T>::lower_bound(T& element, bool /*with_errors=false*/)
{
  SkipListTAcc acc = SkipListTAcc(m_skip_list);
  SkipListTIter iter = acc.lower_bound(element);
  return std::move(SkipListLatencyBufferModel<T>::Iterator(std::move(acc), iter));
}

template<class T>
const T*
SkipListLatencyBufferModel<T>::front()
{
  SkipListTAcc acc(m_skip_list);
  return acc.first();
}

template<class T>
const T*
SkipListLatencyBufferModel<T>::back()
{
  SkipListTAcc acc(m_skip_list);
  return acc.last();
}

template<class T>
void
SkipListLatencyBufferModel<T>::pop(size_t num) // NOLINT(build/unsigned)
{
  {
    SkipListTAcc acc(m_skip_list);
    for (unsigned i = 0; i < num; ++i) {
      acc.pop_back();
    }
  }
}

} // namespace readoutlibs
} // namespace dunedaq
