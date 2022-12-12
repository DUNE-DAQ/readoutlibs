/**
 * @file SkipListLatencyBufferModel.hpp A folly concurrent SkipList wrapper
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_SKIPLISTLATENCYBUFFERMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_SKIPLISTLATENCYBUFFERMODEL_HPP_

#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/ReadoutLogging.hpp"
#include "readoutlibs/concepts/LatencyBufferConcept.hpp"

#include "logging/Logging.hpp"

#include "folly/ConcurrentSkipList.h"

#include <memory>
#include <utility>

using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace readoutlibs {

template<class T>
class SkipListLatencyBufferModel : public LatencyBufferConcept<T>
{

public:
  // Using shorter Folly typenames
  using SkipListT = typename folly::ConcurrentSkipList<T>;
  using SkipListTIter = typename SkipListT::iterator;
  using SkipListTAcc = typename folly::ConcurrentSkipList<T>::Accessor; // SKL Accessor
  using SkipListTSkip = typename folly::ConcurrentSkipList<T>::Skipper; // Skipper accessor

  // Constructor
  SkipListLatencyBufferModel()
    : m_skip_list(folly::ConcurrentSkipList<T>::createInstance(unconfigured_head_height))
  {
    TLOG(TLVL_WORK_STEPS) << "Initializing non configured latency buffer";
  }

  // Iterator for SkipList
  struct Iterator
  {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    Iterator(SkipListTAcc&& acc, SkipListTIter iter)
      : m_acc(std::move(acc))
      , m_iter(iter)
    {
    }

    reference operator*() const { return *m_iter; }
    pointer operator->() { return &(*m_iter); }
    Iterator& operator++() // NOLINT(runtime/increment_decrement) :)
    {
      m_iter++;
      return *this;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_iter == b.m_iter; }
    friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_iter != b.m_iter; }

    bool good() { return m_iter.good(); }

  private:
    SkipListTAcc m_acc;
    SkipListTIter m_iter;
  };

  // Configure
  void conf(const nlohmann::json& /*cfg*/) override
  {
    // Reset datastructure
    m_skip_list = folly::ConcurrentSkipList<T>::createInstance(unconfigured_head_height);
  }

  // Unconfigure
  void scrap(const nlohmann::json& /*args*/) override
  {
    // RS -> Cross-check, we don't need to flush first?
    m_skip_list = folly::ConcurrentSkipList<T>::createInstance(unconfigured_head_height);
  }

  // Get whole skip-list helper function
  std::shared_ptr<SkipListT>& get_skip_list() { return std::ref(m_skip_list); }

  // Override interface implementations
  size_t occupancy() const override;
  void flush() override { pop(occupancy()); }
  bool write(T&& new_element) override;
  bool put(T& new_element); // override
  bool read(T& element) override;

  // Iterator support
  Iterator begin();
  Iterator end();
  Iterator lower_bound(T& element, bool /*with_errors=false*/);

  // Front/back accessors override
  const T* front() override;
  const T* back() override;

  // Pop X override
  void pop(size_t num = 1) override; // NOLINT(build/unsigned)

private:
  // Concurrent SkipList
  std::shared_ptr<SkipListT> m_skip_list;

  // Configuration for datastructure head-hight
  static constexpr uint32_t unconfigured_head_height = 2; // NOLINT(build/unsigned)
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/SkipListLatencyBufferModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_SKIPLISTLATENCYBUFFERMODEL_HPP_
