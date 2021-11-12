/**
 * @file RecorderConcept.hpp Recorder concept
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_RECORDERCONCEPT_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_RECORDERCONCEPT_HPP_

#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "opmonlib/InfoCollector.hpp"
#include "readoutlibs/ReadoutTypes.hpp"
#include "readoutlibs/datarecorder/Structs.hpp"
#include "readoutlibs/utils/BufferedFileWriter.hpp"
#include "readoutlibs/utils/ReusableThread.hpp"

#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace dunedaq {
namespace readoutlibs {

class RecorderConcept
{
public:
  RecorderConcept() {}
  ~RecorderConcept() {}
  RecorderConcept(const RecorderConcept&) = delete;
  RecorderConcept& operator=(const RecorderConcept&) = delete;
  RecorderConcept(RecorderConcept&&) = delete;
  RecorderConcept& operator=(RecorderConcept&&) = delete;

  virtual void init(const nlohmann::json& obj) = 0;
  virtual void get_info(opmonlib::InfoCollector& ci, int level) = 0;

  // Commands
  virtual void do_conf(const nlohmann::json& obj) = 0;
  virtual void do_start(const nlohmann::json& obj) = 0;
  virtual void do_stop(const nlohmann::json& obj) = 0;
};

} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_RECORDERCONCEPT_HPP_
