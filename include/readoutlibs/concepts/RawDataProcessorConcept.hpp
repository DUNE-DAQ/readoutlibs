/**
 * @file RawDataProcessorConcept.hpp Concept of raw data processing
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_RAWDATAPROCESSORCONCEPT_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_RAWDATAPROCESSORCONCEPT_HPP_

#include "opmonlib/InfoCollector.hpp"
#include "daqdataformats/SourceID.hpp"
#include "appdal/ReadoutModule.hpp"

#include <string>
#include <nlohmann/json.hpp>

namespace dunedaq {
namespace readoutlibs {

template<class ReadoutType>
class RawDataProcessorConcept
{
public:
  RawDataProcessorConcept() {}
  virtual ~RawDataProcessorConcept() {}
  
  RawDataProcessorConcept(const RawDataProcessorConcept&) =
    delete; ///< RawDataProcessorConcept is not copy-constructible
  RawDataProcessorConcept& operator=(const RawDataProcessorConcept&) =
    delete;                                                    ///< RawDataProcessorConcept is not copy-assginable
  RawDataProcessorConcept(RawDataProcessorConcept&&) = delete; ///< RawDataProcessorConcept is not move-constructible
  RawDataProcessorConcept& operator=(RawDataProcessorConcept&&) =
    delete; ///< RawDataProcessorConcept is not move-assignable

  //! Init the raw processor
  //virtual void init(const appdal::DataProcessor* conf) {}
  //! Start operation
  virtual void start(const nlohmann::json& args) = 0;
  //! Stop operation
  virtual void stop(const nlohmann::json& args) = 0;
  //! Set the emulator mode, if active, timestamps of processed packets are overwritten with new ones
  virtual void conf(const appdal::ReadoutModule* conf) = 0;
  //! Unconfigure
  virtual void scrap(const nlohmann::json& args) = 0;
  //! Get info from the raw processor
  virtual void get_info(opmonlib::InfoCollector& ci, int level) = 0;
  //! Get newest timestamp of last seen packet
  virtual std::uint64_t get_last_daq_time() = 0; // NOLINT(build/unsigned)
  //! Preprocess one element
  virtual void preprocess_item(ReadoutType* item) = 0;
  //! Postprocess one element
  virtual void postprocess_item(const ReadoutType* item) = 0;
};

} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_CONCEPTS_RAWDATAPROCESSORCONCEPT_HPP_
