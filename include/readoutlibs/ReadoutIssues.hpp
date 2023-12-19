/**
 * @file ReadoutIssues.hpp Readout system related ERS issues
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_READOUTISSUES_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_READOUTISSUES_HPP_

#include "daqdataformats/SourceID.hpp"
#include "daqdataformats/Types.hpp"

#include <ers/Issue.hpp>

#include <string>

namespace dunedaq {
ERS_DECLARE_ISSUE(readoutlibs,
                  InternalError,
                  "SourceID[" << sourceid << "] Internal Error: " << error,
                  ((daqdataformats::SourceID)sourceid)((std::string)error))

ERS_DECLARE_ISSUE(readoutlibs,
                  CommandError,
                  "SourceID[" << sourceid << "] Command Error: " << commanderror,
                  ((daqdataformats::SourceID)sourceid)((std::string)commanderror))

ERS_DECLARE_ISSUE(readoutlibs,
                  InitializationError,
                  "Readout Initialization Error: " << initerror,
                  ((std::string)initerror))

ERS_DECLARE_ISSUE(readoutlibs,
                  ConfigurationError,
                  "SourceID[" << sourceid << "] Readout Configuration Error: " << conferror,
                  ((daqdataformats::SourceID)sourceid)((std::string)conferror))

ERS_DECLARE_ISSUE(readoutlibs,
                  BufferedReaderWriterConfigurationError,
                  "Configuration Error: " << conferror,
                  ((std::string)conferror))

ERS_DECLARE_ISSUE(readoutlibs,
                  DataRecorderConfigurationError,
                  "Configuration Error: " << conferror,
                  ((std::string)conferror))

ERS_DECLARE_ISSUE(readoutlibs,
                  GenericConfigurationError,
                  "Configuration Error: " << conferror,
                  ((std::string)conferror))


ERS_DECLARE_ISSUE(readoutlibs,
                  TimeSyncTransmissionFailed,
                  "SourceID " << sourceid << " failed to send send TimeSync message to " << dest << ".",
                  ((daqdataformats::SourceID)sourceid)((std::string)dest))

ERS_DECLARE_ISSUE(readoutlibs, CannotOpenFile, "Couldn't open binary file: " << filename, ((std::string)filename))

ERS_DECLARE_ISSUE(readoutlibs,
                  BufferedReaderWriterCannotOpenFile,
                  "Couldn't open file: " << filename,
                  ((std::string)filename))

ERS_DECLARE_ISSUE_BASE(readoutlibs,
                       CannotReadFile,
                       readoutlibs::ConfigurationError,
                       " Couldn't read properly the binary file: " << filename << " Cause: " << errorstr,
                       ((daqdataformats::SourceID)sourceid)((std::string)filename),
                       ((std::string)errorstr))

ERS_DECLARE_ISSUE(readoutlibs, CannotWriteToFile, "Could not write to file: " << filename, ((std::string)filename))

ERS_DECLARE_ISSUE(readoutlibs,
                  PostprocessingNotKeepingUp,
                  "SourceID[" << sourceid << "] Postprocessing has too much backlog, thread: " << i,
                  ((daqdataformats::SourceID)sourceid)((size_t)i))

ERS_DECLARE_ISSUE(readoutlibs,
                  EmptySourceBuffer,
                  "SourceID[" << sourceid << "] Source Buffer is empty, check file: " << filename,
                  ((daqdataformats::SourceID)sourceid)((std::string)filename))

ERS_DECLARE_ISSUE(readoutlibs,
                  CannotReadFromQueue,
                  "SourceID[" << sourceid << "] Failed attempt to read from the queue: " << queuename,
                  ((daqdataformats::SourceID)sourceid)((std::string)queuename))

ERS_DECLARE_ISSUE(readoutlibs,
                  CannotWriteToQueue,
                  "SourceID[" << sourceid << "] Failed attempt to write to the queue: " << queuename
                           << ". Data will be lost!",
                  ((daqdataformats::SourceID)sourceid)((std::string)queuename))

ERS_DECLARE_ISSUE(readoutlibs,
                  RequestSourceIDMismatch,
                  "SourceID[" << sourceid << "] Got request for SourceID: " << request_sourceid,
                  ((daqdataformats::SourceID)sourceid)((daqdataformats::SourceID)request_sourceid))


ERS_DECLARE_ISSUE(readoutlibs,
                  TrmWithEmptyFragment,
                  "SourceID[" << sourceid << "] Trigger Matching result with empty fragment: " << trmdetails,
                  ((daqdataformats::SourceID)sourceid)((std::string)trmdetails))

ERS_DECLARE_ISSUE(readoutlibs,
                  RequestOnEmptyBuffer,
                  "SourceID[" << sourceid << "] Request on empty buffer: " << trmdetails,
                  ((daqdataformats::SourceID)sourceid)((std::string)trmdetails))

ERS_DECLARE_ISSUE_BASE(readoutlibs,
                       FailedReadoutInitialization,
                       readoutlibs::InitializationError,
                       " Couldnt initialize Readout with current Init arguments " << initparams << ' ',
                       ((std::string)name),
                       ((std::string)initparams))

ERS_DECLARE_ISSUE(readoutlibs,
                  FailedFakeCardInitialization,
                  "Could not initialize fake card " << name,
                  ((std::string)name))

ERS_DECLARE_ISSUE_BASE(readoutlibs,
                       NoImplementationAvailableError,
                       readoutlibs::ConfigurationError,
                       " No " << impl << " implementation available for raw type: " << rawt << ' ',
                       ((daqdataformats::SourceID)sourceid)((std::string)impl),
                       ((std::string)rawt))

ERS_DECLARE_ISSUE(readoutlibs,
                  ResourceQueueError,
                  " The " << queueType << " queue was not successfully created for " << moduleName,
                  ((std::string)queueType)((std::string)moduleName))

ERS_DECLARE_ISSUE_BASE(readoutlibs,
                       DataRecorderResourceQueueError,
                       readoutlibs::DataRecorderConfigurationError,
                       " The " << queueType << " queue was not successfully created. ",
                       ((std::string)name),
                       ((std::string)queueType))

ERS_DECLARE_ISSUE(readoutlibs,
                  GenericResourceQueueError,
                  "The " << queueType << " queue was not successfully created for " << moduleName,
                  ((std::string)queueType)((std::string)moduleName))

ERS_DECLARE_ISSUE(readoutlibs, ConfigurationNote, "ConfigurationNote: " << text, ((std::string)name)((std::string)text))

ERS_DECLARE_ISSUE(readoutlibs,
                  ConfigurationProblem,
                  "SourceID[" << sourceid << "] Configuration problem: " << text,
                  ((daqdataformats::SourceID)sourceid)((std::string)text))

ERS_DECLARE_ISSUE(readoutlibs,
                  RequestTimedOut,
                  "SourceID[" << sourceid << "] Request timed out",
                  ((daqdataformats::SourceID)sourceid))

ERS_DECLARE_ISSUE(readoutlibs,
                  VerboseRequestTimedOut,
                  "SourceID[" << sourceid << "] Request timed out for trig/seq_num " << trignum << "." << seqnum
                           << ", run_num " << runnum << ", window begin/end " << window_begin << "/" << window_end
                           << ", data_destination: " << dest,
                  ((daqdataformats::SourceID)sourceid)((daqdataformats::trigger_number_t)trignum)((daqdataformats::sequence_number_t)seqnum)((daqdataformats::run_number_t)runnum)((daqdataformats::timestamp_t)window_begin)((daqdataformats::timestamp_t)window_end)((std::string)dest))

ERS_DECLARE_ISSUE(readoutlibs,
                  EndOfRunEmptyFragment,
                  "SourceID[" << sourceid << "] Empty fragment at the end of the run",
                  ((daqdataformats::SourceID)sourceid))

ERS_DECLARE_ISSUE(readoutlibs,
                  DataPacketArrivedTooLate,
                  "Received a late data packet in run " << run << ", payload first timestamp = " << ts1 <<
                  ", request_handler cutoff timestamp = " << ts2 << ", difference = " << tick_diff <<
                  " ticks, " << msec_diff << " msec.",
                  ((daqdataformats::run_number_t)run)((daqdataformats::timestamp_t)ts1)((daqdataformats::timestamp_t)ts2)((int64_t)tick_diff)((double)msec_diff))

} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_READOUTISSUES_HPP_
