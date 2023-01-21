
// <stf_trace_info> -*- C++ -*-

/**
 * \brief  This tool is to add or modify the trace info record in traces.
 *  It should take a trace as input and write a new trace as output with
 *  specified trace info.
 */

#include <cstdint>
#include <map>
#include <iomanip>
#include <iostream>
#include <string>

#include "command_line_parser.hpp"
#include "format_utils.hpp"
#include "stf_reader.hpp"
#include "stf_writer.hpp"
#include "stf_record_types.hpp"
#include "tools_util.hpp"

/**
 * \brief Parse the command line options
 *
 */
static void parseCommandLine(int argc,
                               char **argv,
                               std::string& trace_filename,
                               std::string& output_filename,
                               stf::TraceInfoRecord& trace_info,
                               stf::TraceInfoFeatureRecord& trace_features,
                               bool& show_detail) {
    // Parse options
    trace_tools::CommandLineParser parser("stf_trace_info");
    parser.addFlag('o', "output", " Generate output trace filename with specified trace info. If not specified, show existing trace info.");
    parser.addFlag('g', "generator", "Specify decimal trace generator for output. 1-QEMU, 2-Android Emulator, 3-GEM5");
    parser.addFlag('v', "gen_ver", "Specify trace info generator version for output");
    parser.addFlag('c', "comment", "Specify trace info comment for output");
    parser.addMultiFlag('f', "feature", "Specify 32bit hex value for trace info feature for output");
    parser.addFlag('d', "Show the detailed info, such as trace version, comments etc.");
    parser.addPositionalArgument("trace", "trace in STF format");

    parser.appendHelpText("Trace Feature Codes:");
    parser.appendHelpText("    STF_CONTAIN_PHYSICAL_ADDRESS        0x00001");
    parser.appendHelpText("    STF_CONTAIN_DATA_ATTRIBUTE          0x00002");
    parser.appendHelpText("    STF_CONTAIN_OPERAND_VALUE           0x00004");
    parser.appendHelpText("    STF_CONTAIN_EVENT                   0x00008");
    parser.appendHelpText("    STF_CONTAIN_SYSTEMCALL_VALUE        0x00010");
    parser.appendHelpText("    STF_CONTAIN_INT_DIV_OPERAND_VALUE   0x00040");
    parser.appendHelpText("    STF_CONTAIN_SAMPLING                0x00080");
    parser.appendHelpText("    STF_CONTAIN_EMBEDDED_PTE            0x00100");
    parser.appendHelpText("    STF_CONTAIN_SIMPOINT                0x00200");
    parser.appendHelpText("    STF_CONTAIN_PROCESS_ID              0x00400");
    parser.appendHelpText("    STF_CONTAIN_PTE_ONLY                0x00800");
    parser.appendHelpText("    STF_NEED_POST_PROCESS               0x01000");
    parser.appendHelpText("    STF_CONTAIN_REG_STATE               0x02000");
    parser.appendHelpText("    STF_CONTAIN_MICROOP                 0x04000");
    parser.appendHelpText("    STF_CONTAIN_MULTI_THREAD            0x08000");
    parser.appendHelpText("    STF_CONTAIN_MULTI_CORE              0x10000");
    parser.appendHelpText("    STF_CONTAIN_PTE_HW_AD               0x20000");
    parser.appendHelpText("    STF_CONTAIN_VEC                     0x40000");
    parser.appendHelpText("    STF_CONTAIN_EVENT64                 0x80000");
    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);

    if(stf::STF_GEN stf_gen; parser.getArgumentValue('g', stf_gen)) {
        trace_info.setGenerator(stf_gen);
    }

    if(std::string_view version; parser.getArgumentValue('v', version)) {
        trace_info.setVersion(version);
    }

    if(std::string_view comment; parser.getArgumentValue('c', comment)) {
        trace_info.setComment(comment);
    }

    for(const auto& feature: parser.getMultipleValueArgument('f')) {
        trace_features.setFeature(parseInt<uint64_t, 0>(feature));
    }

    show_detail = parser.hasArgument('d');
    parser.getPositionalArgument(0, trace_filename);

    const auto generator = trace_info.getGenerator();

    // validate arguments;
    if (!output_filename.empty()) {
        if(!stf::STFWriter::isCompressedFile(output_filename)) {
            std::cerr << "Warning: output file format is not compressed." << std::endl;
        }

        parser.assertCondition((generator > stf::STF_GEN::STF_GEN_RESERVED) &&
                               (generator < stf::STF_GEN::STF_GEN_RESERVED_END),
                               "Generator is out of range");

        // In some cases, there is no feature flag.
        //trace_info.features ==0
        parser.assertCondition(trace_info.isVersionSet(), "Need to specify trace info versions and features");

        // check features;
        uint64_t supported = stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_PHYSICAL_ADDRESS)      |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_DATA_ATTRIBUTE)        |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_OPERAND_VALUE)         |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_EVENT)                 |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_SYSTEMCALL_VALUE)      |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_RV64)                  |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_INT_DIV_OPERAND_VALUE) |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_SAMPLING)              |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_PTE)                   |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_SIMPOINT)              |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_PROCESS_ID)            |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_PTE_ONLY)              |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_NEED_POST_PROCESS)             |
                             stf::enums::to_int(stf::TRACE_FEATURES::STF_CONTAIN_REG_STATE);
        if (trace_features.getFeatures() & ~supported) {
            std::cerr << "Warning: Some bits of features are not supported. Ignored!" << std::endl;
        }
    }
    else {
        if ((generator < stf::STF_GEN::STF_GEN_RESERVED_END) && (generator > stf::STF_GEN::STF_GEN_RESERVED)) {
            std::cerr << "Warning: Generator option is ignored because output file is not specified." << std::endl;
        }

        if(trace_info.isVersionSet()) {
            std::cerr << "Warning: Generator version is ignored because output file is not specified." << std::endl;
        }
        if (trace_features.getFeatures() != 0) {
            std::cerr << "Warning: Features option is ignored because output file is not specified." << std::endl;
        }
    }

}

int main (int argc, char **argv) {
    std::string trace_filename;
    std::string output_filename;
    stf::TraceInfoRecord trace_info;
    stf::TraceInfoFeatureRecord trace_features;
    bool show_detail = false;

    try {
        parseCommandLine (argc, argv, trace_filename, output_filename, trace_info, trace_features, show_detail);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    // Open stf trace reader
    const bool enable_writer = !output_filename.empty();
    // Opens in single-threaded mode if we aren't writing anything
    // (so we don't wait around to decompress a chunk we aren't going to use)
    stf::STFReader reader(trace_filename, enable_writer);
    /* FIXME Because we have not kept up with STF versioning, this is currently broken and must be loosened.
    if (!stfr_check_version(reader)) {
        stfr_close(reader);
        exit(1);
    }
    */

    stf::STFWriter writer;
    if (enable_writer) {
        writer.open(output_filename);
    }

    if (show_detail) {
        stf::format_utils::formatLabel(std::cerr, "VERSION");
        std::cerr << reader.major() << '.' << reader.minor() << std::endl;
    }

    stf::STFRecord::UniqueHandle rec;
    std::unique_ptr<stf::STFRecord> output_rec;
    // current trace
    if(writer) {
        reader.copyHeader(writer);
        writer.addTraceInfo(trace_info);
        writer.setTraceFeature(trace_features.getFeatures());
        writer.finalizeHeader();
    }
    else {
        for(const auto& i: reader.getTraceInfo()) {
            std::cerr << *i << std::endl;
        }
        if(const auto& features = reader.getTraceFeatures()) {
            std::cerr << *features << std::endl;
        }
    }

    if(writer) {
        while (reader >> rec) {
            writer << *rec;
        }
    }

    reader.close();

    if (writer) {
        writer.close();
    }

    return 0;
}
