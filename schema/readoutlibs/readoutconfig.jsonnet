// The schema used by classes in the appfwk code tests.
//
// It is an example of the lowest layer schema below that of the "cmd"
// and "app" and which defines the final command object structure as
// consumed by instances of specific DAQModule implementations (ie,
// the test/Fake* modules).

local moo = import "moo.jsonnet";

// A schema builder in the given path (namespace)
local ns = "dunedaq.readoutlibs.readoutconfig";
local s = moo.oschema.schema(ns);

// Object structure used by the test/fake producer module
local readoutconfig = {
    size: s.number("Size", "u8",
                   doc="A count of very many things"),

    source_id: s.number("SourceID_t", "u4",
                         doc="A source id (as part of a SourceID)"),

    det_id: s.number("DetID_t", "u2",
                         doc="A detector id"),

    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    pct : s.number("Percent", "f4",
                   doc="Testing float number"),

    choice : s.boolean("Choice"),

    file_name : s.string("FileName",
                      doc="A string field"),

    string : s.string("String", moo.re.ident,
                      doc="A string field"),

    netmgr_name : s.string("NetworkManagerName", doc="Connection or topic name to be used with NetworkManager"),

    latencybufferconf : s.record("LatencyBufferConf", [
            s.field("latency_buffer_size", self.size, 100000,
                            doc="Size of latency buffer"),
            s.field("latency_buffer_numa_aware", self.choice, false,
                            doc="Use numa allocation for LB"),
            s.field("latency_buffer_numa_node", self.count, 0,
                            doc="NUMA node to use for allocation if latency_buffer_numa_aware is set to true"),
            s.field("latency_buffer_intrinsic_allocator", self.choice, false,
                            doc="Use intrinsic allocator for LB"),
            s.field("latency_buffer_alignment_size", self.count, 0,
                            doc="Alignment size of LB allocation"),
            s.field("latency_buffer_preallocation", self.choice, false,
                            doc="Preallocate memory for the latency buffer"),
            s.field("source_id", self.source_id, 0,
                            doc="The source id of this link")],
            doc="Latency Buffer Config"),

    rawdataprocessorconf : s.record("RawDataProcessorConf", [
            s.field("postprocess_queue_sizes", self.size, 10000,
                            doc="Size of the queues used for postprocessing"),
            s.field("tp_timeout", self.size, 100000,
                            doc="Timeout after which ongoing TPs are discarded"),
            s.field("tpset_window_size", self.size, 10000,
                            doc="Size of the TPSet windows"),
            s.field("channel_map_rce", self.file_name, "",
                            doc="Channel map rce file for software TPG. If empty string, look in $READOUT_SHARE"),
            s.field("channel_map_felix", self.file_name, "",
                            doc="Channel map felix file for software TPG. If empty string, look in $READOUT_SHARE"),
            s.field("enable_software_tpg", self.choice, false,
                            doc="Enable software TPG"),
            s.field("enable_firmware_tpg", self.choice, false,
                            doc="Enable firmware TPG"),
            s.field("fwtp_number_of_ticks", self.size, 64,
                            doc="Number ticks over which a firmware TP frame spans"),
            s.field("fwtp_tick_length", self.size, 32,
                            doc="Tick length as defined by DUNE WIB"),
            s.field("fwtp_fake_timestamp", self.choice, false,
                            doc="calculate a fake timestamp when processing firmware tps"),
            s.field("fwtp_format_version", self.size, 1,
                            doc="Format version of raw TP frames from firmware TPG"),
            s.field("channel_map_name", self.string, default="None",
                            doc="Name of channel map"),
            s.field("emulator_mode", self.choice, false,
                            doc="If the input data is from an emulator."),
            s.field("source_id", self.source_id, 0,
                            doc="The source id of this link"),
            s.field("error_counter_threshold", self.size, 100,
                            doc="Maximum number of frames queued per error type"),
            s.field("error_reset_freq", self.size, 10000,
                            doc="Number of processed frames to allow errored frames pushed to queue"),
            s.field("tpset_sourceid", self.source_id, 0, doc="The source id of TPSets from this link")],
            doc="RawDataProcessor Config"),

    requesthandlerconf : s.record("RequestHandlerConf", [
            s.field("num_request_handling_threads", self.count, 4,
                            doc="Number of threads to use for data request handling"),
            s.field("request_timeout_ms", self.count, 1000,
                            doc="Timeout for checking for valid data in response to a request before sending an empty fragment"),
            s.field("output_file", self.file_name, "output.out",
                            doc="Name of the output file to write to"),
            s.field("stream_buffer_size", self.size, 8388608,
                            doc="Buffer size of the stream buffer"),
            s.field("compression_algorithm", self.string, "None",
                            doc="Compression algorithm to use before writing to file"),
            s.field("use_o_direct", self.choice, true,
                            doc="Whether to use O_DIRECT flag when opening files"),
            s.field("enable_raw_recording", self.choice, true,
                            doc="Enable raw recording"),
            s.field("fragment_queue_timeout_ms", self.count, 100,
                            doc="Timeout for pushing to the fragment queue"),
            s.field("pop_limit_pct", self.pct, 0.5,
                            doc="Latency buffer occupancy percentage to issue an auto-pop"),
            s.field("pop_size_pct", self.pct, 0.8,
                            doc="Percentage of current occupancy to pop from the latency buffer"),
            s.field("latency_buffer_size", self.size, 100000,
                            doc="Size of latency buffer"),
            s.field("source_id", self.source_id, 0,
                            doc="The source id of this link"),
            s.field("det_id", self.det_id, 1,
                            doc="The det id of data carried by this link"),
            s.field("warn_on_timeout", self.choice, true,
                            doc="Whether to emit a warning when a request times out"),
            s.field("warn_about_empty_buffer", self.choice, true,
                            doc="Whether to emit a warning when there is no data in the buffer when a request is processed")],
            doc="Request Handler Config"),

    readoutmodelconf : s.record("ReadoutModelConf", [
            s.field("fake_trigger_flag", self.count, 0,
                            doc="flag indicating whether to generate fake triggers: 1=true, 0=false "),
            s.field("source_queue_timeout_ms", self.count, 2000,
                            doc="Timeout for source queue"),
            s.field("source_id", self.source_id, 0,
                            doc="The source id of this link"),
            s.field("timesync_connection_name", self.netmgr_name, "", doc="Connection name for sending timesyncs"),
            s.field("send_partial_fragment_if_available", self.choice, false,
                            doc="Whether to send a partial fragment if one is available")
    ], doc="Readout Model Config"),

    conf: s.record("Conf", [
        s.field("latencybufferconf", self.latencybufferconf, doc="Latency Buffer config"),
        s.field("rawdataprocessorconf", self.rawdataprocessorconf, doc="RawDataProcessor config"),
        s.field("requesthandlerconf", self.requesthandlerconf, doc="Request Handler config"),
        s.field("readoutmodelconf", self.readoutmodelconf, doc="Readout Model config")

    ], doc="Generic readout element configuration"),

    recording: s.record("RecordingParams", [
        s.field("duration", self.count, 1,
                doc="Number of seconds to record")
    ], doc="Recording parameters"),

};

moo.oschema.sort_select(readoutconfig, ns)
