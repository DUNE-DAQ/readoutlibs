/**
 * @file ReadoutConcept.hpp ReadoutConcept for constructors and
 * forwarding command args.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_READOUTCONCEPT_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_READOUTCONCEPT_HPP_

#include "opmonlib/InfoCollector.hpp"
#include "appdal/ReadoutModule.hpp"

namespace dunedaq {
namespace readoutlibs {

class ReadoutConcept
{
public:
  ReadoutConcept() {}
  virtual ~ReadoutConcept() {}
  ReadoutConcept(const ReadoutConcept&) = delete;            ///< ReadoutConcept is not copy-constructible
  ReadoutConcept& operator=(const ReadoutConcept&) = delete; ///< ReadoutConcept is not copy-assginable
  ReadoutConcept(ReadoutConcept&&) = delete;                 ///< ReadoutConcept is not move-constructible
  ReadoutConcept& operator=(ReadoutConcept&&) = delete;      ///< ReadoutConcept is not move-assignable

  //! Forward calls from the appfwk
  virtual void init(const appdal::ReadoutModule* mcfg) = 0;
  virtual void conf(const nlohmann::json& args) = 0;
  virtual void scrap(const nlohmann::json& args) = 0;
  virtual void start(const nlohmann::json& args) = 0;
  virtual void stop(const nlohmann::json& args) = 0;
  virtual void get_info(opmonlib::InfoCollector& ci, int level) = 0;
  virtual void record(const nlohmann::json& args) = 0;

  //! Function that will be run in its own thread to read the raw packets from the connection and add them to the LB
  virtual void run_consume() = 0;
  //! Function that will be run in its own thread and sends periodic timesync messages by pushing them to the connection
  virtual void run_timesync() = 0;
};

} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_READOUTCONCEPT_HPP_
