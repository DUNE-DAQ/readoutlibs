/**
 * @file FixedRateQueueModel.hpp Queue that can be searched
 *
 * This is part of the DUNE DAQ , copyright 2021.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_FIXEDRATEQUEUEMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_FIXEDRATEQUEUEMODEL_HPP_

#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/ReadoutLogging.hpp"

#include "logging/Logging.hpp"

#include "BinarySearchQueueModel.hpp"

namespace dunedaq {
namespace readoutlibs {

template<class T>
class FixedRateQueueModel : public BinarySearchQueueModel<T>
{
public:
  FixedRateQueueModel()
    : BinarySearchQueueModel<T>()
  {
  }

  explicit FixedRateQueueModel(uint32_t size) // NOLINT(build/unsigned)
    : BinarySearchQueueModel<T>(size)
  {
  }

  typename IterableQueueModel<T>::Iterator lower_bound(T& element, bool with_errors = false);
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/FixedRateQueueModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_FIXEDRATEQUEUEMODEL_HPP_
