// The schema used by classes in the appfwk code tests.
//
// It is an example of the lowest layer schema below that of the "cmd"
// and "app" and which defines the final command object structure as
// consumed by instances of specific DAQModule implementations (ie,
// the test/Fake* modules).

local moo = import "moo.jsonnet";

// A schema builder in the given path (namespace)
local ns = "dunedaq.readoutlibs.sourceemulatorconfig";
local s = moo.oschema.schema(ns);

// Object structure used by the test/fake producer module
local sourceemulatorconfig = {
    size: s.number("Size", "u8",
                   doc="A count of very many things"),

    uint4  : s.number("uint4", "u4",
                     doc="An unsigned of 4 bytes"),

    int8  : s.number("int8", "i8",
                     doc="integer of 8 bytes"),

    khz  : s.number("khz", "f8",
                     doc="A frequency in kHz"),

    double8 : s.number("double8", "f8",
                     doc="floating point of 8 bytes"),

    float4 : s.number("float4", "f4", doc="A float of 4 bytes"),

    slowdown_t : s.number("slowdown_t", "f8",
                     doc="Slowdown factor"),
  
    source_id: s.number("source_id_t", "u4"),

    string : s.string("FilePath", moo.re.hierpath,
                  doc="A string field"),

    choice : s.boolean("Choice"),

    tp_enabled: s.string("TpEnabled", moo.re.ident,
                  doc="A true or false flag for enabling raw WIB TP link"),

    link_conf : s.record("LinkConfiguration", [
        s.field("source_id", self.source_id, doc="SourceID of the link"),
        s.field("crate_id", self.uint4, 0,
                            doc="The crate id of this link"),
        s.field("slot_id", self.uint4, 0,
                            doc="The slot id of this link"),
        s.field("link_id", self.uint4, 0,
                            doc="The link id of this link"),
        s.field("input_limit", self.uint4, 10485100,
            doc="Maximum allowed file size"),
        s.field("slowdown", self.slowdown_t, 1,
            doc="Slowdown factor"),
        s.field("data_filename", self.string, "/tmp/frames.bin",
            doc="Data file that contains user payloads"),
        s.field("tp_data_filename", self.string, "/tmp/tp_frames.bin",
            doc="Data file that contains raw WIB TP user payloads"),
        s.field("queue_name", self.string,
            doc="Name of the output queue"),
        s.field("random_population_size", self.uint4, 10000,
            doc="Size of the random population"),
        s.field("emu_frame_error_rate", self.double8, 0.0,
            doc="Rate of faked errors in frame header"),
        ], doc="Configuration for one link"),

    link_conf_list : s.sequence("link_conf_list", self.link_conf, doc="Link configuration list"),

    conf: s.record("Conf", [
        s.field("link_confs", self.link_conf_list,
                doc="Link configurations"),

        s.field("queue_timeout_ms", self.uint4, 2000,
                doc="Queue timeout in milliseconds"),

        s.field("use_now_as_first_data_time", self.choice, false,
                doc="Whether to use the current wallclock time for the timestamp of the first data frame"),

        s.field("clock_speed_hz", self.size, 62500000,
                doc="Clock frequency in Hz (for use in calculating the first data frame timestamp)"),

        s.field("set_t0_to", self.int8, -1,
                doc="The first DAQ timestamp. If -1, t0 from file is used."),
        
        s.field( "generate_periodic_adc_pattern", self.choice, false, 
                doc="Generate a periodic ADC pattern inside the input data."),                

        s.field( "TP_rate_per_ch", self.float4, 1.0,
                doc="Rate of TPs per channel when using a periodic ADC pattern generation. Values expresses as multiples of the expected rate of 100 Hz/ch.")

    ], doc="Fake Elink reader module configuration"),

};

moo.oschema.sort_select(sourceemulatorconfig, ns)
