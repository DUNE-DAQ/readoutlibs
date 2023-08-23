// This is the application info schema used by the data link handler module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.readoutlibs.readoutinfo");

local info = {
    int2   : s.number("int2", "i2", doc="An signed of 2 bytes"),
    uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),
    float8 : s.number("float8", "f8", doc="A float of 8 bytes"),
    choice : s.boolean("Choice"),
    string : s.string("String", moo.re.ident, doc="A string field"),

   #latencybufferinfo: s.record("LatencyBufferInfo", [

   #], doc="Latency buffer information"),

   rawdataprocessorinfo: s.record("RawDataProcessorInfo", [
        s.field("num_tps_sent",                  self.uint8,     0, doc="Number of sent TPs"),
        s.field("num_tpsets_sent",               self.uint8,     0, doc="Number of sent TPSets"),
        s.field("num_tps_dropped",               self.uint8,     0, doc="Number of dropped TPs (because they were too old)"),
        s.field("num_heartbeats",                self.uint8,     0, doc="Number of empty TP sets - heartbeats"),
        s.field("rate_tp_hits",                  self.float8,    0, doc="TP hit rate in kHz"),
        s.field("num_frame_errors",              self.uint8,     0, doc="Total number of frame errors"),
        s.field("num_seq_id_errors",             self.uint8,     0, doc="Total number of sequence id frame errors"),
        s.field("min_seq_id_jump",               self.int2,      0, doc="Minimum sequence-id jump"),
        s.field("max_seq_id_jump",               self.int2,      0, doc="Maximum sequence-id jump"),
        s.field("num_ts_errors",                 self.uint8,     0, doc="Total number of timestamp frame errors")
   ], doc="Latency buffer information"),

   tpchannelinfo: s.record("TPChannelInfo", [
        s.field("num_tp",                  self.uint8,     0, doc="Number of TPs per channel")
   ], doc="Info for a single TP channel"),

   requesthandlerinfo: s.record("RequestHandlerInfo", [
        s.field("num_requests_found",            self.uint8,     0, doc="Number of found requests"),
        s.field("num_requests_bad",              self.uint8,     0, doc="Number of bad requests"),
        s.field("num_requests_old_window",       self.uint8,     0, doc="Number of requests with data that is too old"),
        s.field("num_requests_delayed",          self.uint8,     0, doc="Number of delayed requests (data not there yet)"),
        s.field("num_requests_uncategorized",    self.uint8,     0, doc="Number of uncategorized requests"),
        s.field("num_requests_timed_out",        self.uint8,     0, doc="Number of timed out requests"),
        s.field("num_requests_waiting",          self.uint8,     0, doc="Number of waiting requests"),
        s.field("num_buffer_cleanups",           self.uint8,     0, doc="Number of latency buffer cleanups"),
        s.field("recording_status",              self.string,    0, doc="Recording status"),
        s.field("avg_request_response_time",     self.uint8,     0, doc="Average response time in us"),
        s.field("tot_request_response_time",     self.uint8,     0, doc="Total response time in us for the requests handled in between get_info calls"),
        s.field("min_request_response_time",     self.uint8,     0, doc="Min response time in us for the requests handled in between get_info calls"),
        s.field("max_request_response_time",     self.uint8,     0, doc="Max response time in us for the requests handled in between get_info calls"),
        s.field("num_requests_handled",          self.uint8,     0, doc="Number of requests handled in between get_info calls"),
        s.field("is_recording",                  self.choice,    0, doc="If the DLH is recording"),
        s.field("num_payloads_written",          self.uint8,     0, doc="Number of payloads written in the recording")
   ], doc="Request Handler information"),

   readoutlibsinfo: s.record("ReadoutInfo", [
       s.field("sum_payloads",                  self.uint8,     0, doc="Total number of received payloads"),
       s.field("num_payloads",                  self.uint8,     0, doc="Number of received payloads"),
       s.field("sum_requests",                  self.uint8,     0, doc="Total number of received requests"),
       s.field("num_requests",                  self.uint8,     0, doc="Number of received requests"),
       s.field("num_payloads_overwritten",      self.uint8,     0, doc="Number of overwritten payloads because the LB is full"),
       s.field("rate_payloads_consumed",        self.float8,    0, doc="Rate of consumed packets"),
       s.field("num_raw_queue_timeouts",        self.uint8,     0, doc="Raw queue timeouts"),
       s.field("num_buffer_elements",           self.uint8,     0, doc="Occupancy of the LB"),
       s.field("last_daq_timestamp",            self.uint8,     0, doc="Last DAQ timestamp processed")
   ], doc="Readout information")
};

moo.oschema.sort_select(info) 
