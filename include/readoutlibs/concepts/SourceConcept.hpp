/**
 * @file SourceConcept.hpp SourceConcept for constructors and
 * forwarding command args. Enforces the implementation to
 * queue in data frames to be translated to TypeAdapters and
 * send them to corresponding sinks.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_SRC_SOURCECONCEPT_HPP_
#define READOUTLIBS_SRC_SOURCECONCEPT_HPP_


#include "appfwk/DAQModule.hpp"
#include "coredal/DaqModule.hpp"

#include <memory>
#include <sstream>
#include <string>

namespace dunedaq {
namespace readoutlibs {

class SourceConcept
{
public:
  SourceConcept() {}
  virtual ~SourceConcept() {}

  SourceConcept(const SourceConcept&) = delete;            ///< SourceConcept is not copy-constructible
  SourceConcept& operator=(const SourceConcept&) = delete; ///< SourceConcept is not copy-assginable
  SourceConcept(SourceConcept&&) = delete;                 ///< SourceConcept is not move-constructible
  SourceConcept& operator=(SourceConcept&&) = delete;      ///< SourceConcept is not move-assignable

  virtual void init(const coredal::DaqModule* mcfg) = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void get_info(opmonlib::InfoCollector& ci, int level) = 0;
  //virtual bool handle_payload(T& data) = 0;
  
};

} // namespace READOUTLIBS
} // namespace dunedaq

#endif // READOUTLIBS_SRC_SOURCECONCEPT_HPP_
