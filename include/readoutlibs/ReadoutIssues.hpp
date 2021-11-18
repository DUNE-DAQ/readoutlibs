/**
 * @file ReadoutIssues.hpp Readout system related ERS issues
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_READOUTISSUES_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_READOUTISSUES_HPP_

#include "daqdataformats/GeoID.hpp"

#include <ers/Issue.hpp>

#include <string>

namespace dunedaq {
ERS_DECLARE_ISSUE(readoutlibs,
                  FragmentTransmissionFailed,
                  "GeoID" << geoid << " failed to send data for trigger number " << tr_num << ".",
                  ((daqdataformats::GeoID)geoid)((int64_t)tr_num))

ERS_DECLARE_ISSUE(readoutlibs,
                  TimeSyncTransmissionFailed,
                  "GeoID " << geoid << " failed to send send TimeSync message to " << dest << " with topic " << topic
                           << ".",
                  ((daqdataformats::GeoID)geoid)((std::string)dest)((std::string)topic))

ERS_DECLARE_ISSUE(readoutlibs,
                  InternalError,
                  "GeoID[" << geoid << "] Internal Error: " << error,
                  ((daqdataformats::GeoID)geoid)((std::string)error))

ERS_DECLARE_ISSUE(readoutlibs,
                  CommandError,
                  "GeoID[" << geoid << "] Command Error: " << commanderror,
                  ((daqdataformats::GeoID)geoid)((std::string)commanderror))

ERS_DECLARE_ISSUE(readoutlibs,
                  InitializationError,
                  "Readout Initialization Error: " << initerror,
                  ((std::string)initerror))

ERS_DECLARE_ISSUE(readoutlibs,
                  ConfigurationError,
                  "GeoID[" << geoid << "] Readout Configuration Error: " << conferror,
                  ((daqdataformats::GeoID)geoid)((std::string)conferror))

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
                  "GeoID " << geoid << " failed to send send TimeSync message to " << dest << " with topic " << topic
                           << ".",
                  ((daqdataformats::GeoID)geoid)((std::string)dest)((std::string)topic))

ERS_DECLARE_ISSUE(readoutlibs, CannotOpenFile, "Couldn't open binary file: " << filename, ((std::string)filename))

ERS_DECLARE_ISSUE(readoutlibs,
                  BufferedReaderWriterCannotOpenFile,
                  "Couldn't open file: " << filename,
                  ((std::string)filename))

ERS_DECLARE_ISSUE_BASE(readoutlibs,
                       CannotReadFile,
                       readoutlibs::ConfigurationError,
                       " Couldn't read properly the binary file: " << filename << " Cause: " << errorstr,
                       ((daqdataformats::GeoID)geoid)((std::string)filename),
                       ((std::string)errorstr))

ERS_DECLARE_ISSUE(readoutlibs, CannotWriteToFile, "Could not write to file: " << filename, ((std::string)filename))

ERS_DECLARE_ISSUE(readoutlibs,
                  PostprocessingNotKeepingUp,
                  "GeoID[" << geoid << "] Postprocessing has too much backlog, thread: " << i,
                  ((daqdataformats::GeoID)geoid)((size_t)i))

ERS_DECLARE_ISSUE(readoutlibs,
                  EmptySourceBuffer,
                  "GeoID[" << geoid << "] Source Buffer is empty, check file: " << filename,
                  ((daqdataformats::GeoID)geoid)((std::string)filename))

ERS_DECLARE_ISSUE(readoutlibs,
                  CannotReadFromQueue,
                  "GeoID[" << geoid << "] Failed attempt to read from the queue: " << queuename,
                  ((daqdataformats::GeoID)geoid)((std::string)queuename))

ERS_DECLARE_ISSUE(readoutlibs,
                  CannotWriteToQueue,
                  "GeoID[" << geoid << "] Failed attempt to write to the queue: " << queuename
                           << ". Data will be lost!",
                  ((daqdataformats::GeoID)geoid)((std::string)queuename))

ERS_DECLARE_ISSUE(readoutlibs,
                  RequestGeoIDMismatch,
                  "GeoID[" << geoid << "] Got request for GeoID: " << request_geoid,
                  ((daqdataformats::GeoID)geoid)((daqdataformats::GeoID)request_geoid))


ERS_DECLARE_ISSUE(readoutlibs,
                  TrmWithEmptyFragment,
                  "GeoID[" << geoid << "] Trigger Matching result with empty fragment: " << trmdetails,
                  ((daqdataformats::GeoID)geoid)((std::string)trmdetails))

ERS_DECLARE_ISSUE(readoutlibs,
                  RequestOnEmptyBuffer,
                  "GeoID[" << geoid << "] Request on empty buffer: " << trmdetails,
                  ((daqdataformats::GeoID)geoid)((std::string)trmdetails))

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
                       ((daqdataformats::GeoID)geoid)((std::string)impl),
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
                  "GeoID[" << geoid << "] Configuration problem: " << text,
                  ((daqdataformats::GeoID)geoid)((std::string)text))

ERS_DECLARE_ISSUE(readoutlibs,
                  RequestTimedOut,
                  "GeoID[" << geoid << "] Request timed out",
                  ((daqdataformats::GeoID)geoid))

ERS_DECLARE_ISSUE(readoutlibs,
                  EndOfRunEmptyFragment,
                  "GeoID[" << geoid << "] Empty fragment at the end of the run",
                  ((daqdataformats::GeoID)geoid))

} // namespace dunedaq

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_READOUTISSUES_HPP_
