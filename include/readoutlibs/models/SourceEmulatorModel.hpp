/**
 * @file SourceEmulatorModel.hpp Emulates a source with given raw type
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_SOURCEEMULATORMODEL_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_SOURCEEMULATORMODEL_HPP_

#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"

#include "logging/Logging.hpp"

#include "opmonlib/InfoCollector.hpp"

#include "readoutlibs/sourceemulatorconfig/Nljs.hpp"
#include "readoutlibs/sourceemulatorinfo/InfoNljs.hpp"

#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/concepts/SourceEmulatorConcept.hpp"
#include "readoutlibs/utils/ErrorBitGenerator.hpp"
#include "readoutlibs/utils/FileSourceBuffer.hpp"
#include "readoutlibs/utils/RateLimiter.hpp"
#include "readoutlibs/utils/ReusableThread.hpp"

#include "unistd.h"
#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

using dunedaq::readoutlibs::logging::TLVL_TAKE_NOTE;
using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace readoutlibs {


// Pattern generator class for creating different types of TP patterns
// AAA: at the moment the pattern generator is very simple: random source id and random channel
class SourceEmulatorPatternGenerator {

public:
  SourceEmulatorPatternGenerator() {
    m_size = 1000000;
  };
  ~SourceEmulatorPatternGenerator() {};

  void generate(int);

  std::vector<int> get_channels() {
    return m_channel;
  }

  int get_total_size() {
    return m_size;
  }
 
private: 
  int m_size;
  std::vector<int> m_channel;
};


template<class ReadoutType>
class SourceEmulatorModel : public SourceEmulatorConcept
{
public:
  explicit SourceEmulatorModel(std::string name,
                               std::atomic<bool>& run_marker,
                               uint64_t time_tick_diff, // NOLINT(build/unsigned)
                               double dropout_rate,
                               double frame_error_rate,
                               double rate_khz,
			       uint16_t frames_per_tick=1)
    : m_run_marker(run_marker)
    , m_time_tick_diff(time_tick_diff)
    , m_dropout_rate(dropout_rate)
    , m_frame_error_rate(frame_error_rate)
    , m_packet_count{ 0 }
    , m_raw_sender_timeout_ms(0)
    , m_raw_data_sender(nullptr)
    , m_producer_thread(0)
    , m_name(name)
    , m_rate_khz(rate_khz)
    ,m_frames_per_tick(frames_per_tick)
  {}

  void init(const nlohmann::json& /*args*/) {}
  void set_sender(const std::string& conn_name);

  void conf(const nlohmann::json& args, const nlohmann::json& link_conf);
  void scrap(const nlohmann::json& /*args*/)
  {
    m_file_source.reset();
    m_is_configured = false;
  }
  bool is_configured() override { return m_is_configured; }

  void start(const nlohmann::json& /*args*/);
  void stop(const nlohmann::json& /*args*/);
  void get_info(opmonlib::InfoCollector& ci, int /*level*/);

protected:
  // The data emulator function that the worker thread runs
  void run_produce();

private:
  // Constuctor params
  std::atomic<bool>& m_run_marker;

  // CONFIGURATION
  uint32_t m_this_apa_number;  // NOLINT(build/unsigned)
  uint32_t m_this_link_number; // NOLINT(build/unsigned)

  uint64_t m_time_tick_diff; // NOLINT(build/unsigned)
  double m_dropout_rate;
  double m_frame_error_rate;

  // STATS
  std::atomic<int> m_packet_count{ 0 };
  std::atomic<int> m_packet_count_tot{ 0 };

  sourceemulatorconfig::Conf m_cfg;

  // RAW SENDER
  std::chrono::milliseconds m_raw_sender_timeout_ms;
  using raw_sender_ct = iomanager::SenderConcept<ReadoutType>;
  std::shared_ptr<raw_sender_ct> m_raw_data_sender;

  bool m_sender_is_set = false;
  using module_conf_t = dunedaq::readoutlibs::sourceemulatorconfig::Conf;
  module_conf_t m_conf;
  using link_conf_t = dunedaq::readoutlibs::sourceemulatorconfig::LinkConfiguration;
  link_conf_t m_link_conf;

  std::unique_ptr<RateLimiter> m_rate_limiter;
  std::unique_ptr<FileSourceBuffer> m_file_source;
  ErrorBitGenerator m_error_bit_generator;

  ReusableThread m_producer_thread;

  std::string m_name;
  bool m_is_configured = false;
  double m_rate_khz;
  uint16_t m_frames_per_tick;

  std::vector<bool> m_dropouts; // Random population
  std::vector<bool> m_frame_errors;

  uint m_dropouts_length = 10000; // NOLINT(build/unsigned) Random population size
  uint m_frame_errors_length = 10000;
  daqdataformats::SourceID m_sourceid;
  int m_crateid = 0;
  int m_slotid = 0;
  int m_linkid = 0;

  // Pattern generator configs
  SourceEmulatorPatternGenerator m_pattern_generator;
  std::vector<int> m_random_channels; 
  int m_pattern_index = 0;
  uint64_t m_pattern_generator_previous_ts = 0;  
  // Adding a hit every 9768 gives a TP rate of approx 100 Hz/wire using WIBEthernet
  uint32_t m_time_to_wait = 9768; 
};

} // namespace readoutlibs
} // namespace dunedaq

// Declarations
#include "detail/SourceEmulatorModel.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_MODELS_SOURCEEMULATORMODEL_HPP_
