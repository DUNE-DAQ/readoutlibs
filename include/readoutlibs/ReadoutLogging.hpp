/**
 * @file ReadoutLogging.hpp Common logging declarations in readoutlibs
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_READOUTLOGGING_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_READOUTLOGGING_HPP_

namespace dunedaq {
namespace readoutlibs {
namespace logging {

/**
 * @brief Common name used by TRACE TLOG calls from this package
 */
enum
{
  TLVL_HOUSEKEEPING = 1,
  TLVL_TAKE_NOTE = 2,
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10,
  TLVL_QUEUE_PUSH = 11,
  TLVL_QUEUE_POP = 12,
  TLVL_BOOKKEEPING = 15,
  TLVL_TIME_SYNCS = 17
};

} // namespace logging
} // namespace readoutlibs
} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_READOUTLOGGING_HPP_
