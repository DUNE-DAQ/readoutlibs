/**
 * @file DataSubscriberConcept.hpp DataSubscriberConcept for constructors and
 * forwarding command args.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_DATASUBSCRIBERCONCEPT_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_DATASUBSCRIBERCONCEPT_HPP_

#include "opmonlib/InfoCollector.hpp"
#include "appdal/DataSubscriber.hpp"

namespace dunedaq {
namespace readoutlibs {

class DataSubscriberConcept
{
public:
  DataSubscriberConcept() {}
  virtual ~DataSubscriberConcept() {}
  DataSubscriberConcept(const DataSubscriberConcept&) = delete;            ///< DataSubscriberConcept is not copy-constructible
  DataSubscriberConcept& operator=(const DataSubscriberConcept&) = delete; ///< DataSubscriberConcept is not copy-assginable
  DataSubscriberConcept(DataSubscriberConcept&&) = delete;                 ///< DataSubscriberConcept is not move-constructible
  DataSubscriberConcept& operator=(DataSubscriberConcept&&) = delete;      ///< DataSubscriberConcept is not move-assignable

  //! Forward calls from the appfwk
  virtual void init(const appdal::DataSubscriber* mcfg) = 0;
  virtual void start(const nlohmann::json& args) = 0;
  virtual void stop(const nlohmann::json& args) = 0;
  virtual void get_info(opmonlib::InfoCollector& ci, int level) = 0;

  //! Function that will be run in its own thread to read the packets from the connection and pass them on to the Readout modules
  virtual void dispatch() = 0;
};

} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_DATASUBSCRIBERCONCEPT_HPP_
