
// <STF_imem> -*- C++ -*-

/**
 * \brief  This tool prints out instruction access counts
 *
 */

#include <iomanip>
#include <iostream>

#include "command_line_parser.hpp"
#include "stf_imem.hpp"

/**
 * \brief Parse the command line options
 *
 */
static STFImemConfig parse_command_line (int argc, char **argv) {
    STFImemConfig config;
    trace_tools::CommandLineParser parser("stf_imem");
    parser.addFlag('p', "show the inst count percentage");
    parser.addFlag('A', "use aliases for disassembly");
    parser.addFlag('s', "n", "skip the first n instructions");
    parser.addFlag('t', "n", "Count all records with thread ID <n>");
    parser.addFlag('g', "n", "Count all records with process ID (i.e., TGID) <n>");
    parser.addFlag('j', "Java");
    parser.addFlag('P', "Show physical address");
    parser.addFlag('w', "n", "warmup for trace");
    parser.addFlag('r', "n", "runlength for trace");
    parser.addFlag('o', "filename", "write output to <filename>. Defaults to stdout.");
    parser.addFlag('S', "sort output from largest to smallest instruction count");
    parser.addFlag('u', "skip non-user mode instructions");
    parser.addFlag('l', "local history data for branches & load/store strides");
    parser.addPositionalArgument("trace", "trace in STF format");

    parser.parseArguments(argc, argv);

    config.use_aliases = parser.hasArgument('A');
    parser.getArgumentValue('s', config.skip_count);
    config.java_trace = parser.hasArgument('j');
    parser.getArgumentValue<uint32_t, 0>('t', config.g_tid);
    parser.getArgumentValue<uint32_t, 0>('g', config.g_tid);
    config.show_percentage = parser.hasArgument('p');
    config.show_physpc = parser.hasArgument('P');
    const bool has_r = parser.getArgumentValue('r', config.keep_count);

    if(has_r) {
        config.runlength_count = config.keep_count - config.warmup_count;
    }

    const bool has_w = parser.getArgumentValue('w', config.warmup_count);
    config.track = has_r || has_w;
    config.local_history = parser.hasArgument('l');
    if(parser.hasArgument('o')) {
        parser.getArgumentValue('o', config.output_filename);
    }
    else {
        config.output_filename = "-"; // default to stdout
    }
    config.sort_output = parser.hasArgument('S');
    config.skip_non_user = parser.hasArgument('u');

    parser.getPositionalArgument(0, config.trace_filename);

    return config;
}

int main (int argc, char **argv) {
    try {
        const STFImemConfig config = parse_command_line (argc, argv);

        /* FIXME Because we have not kept up with STF versioning, this is currently broken and must be loosened.
        if (!stf_reader.checkVersion()) {
            exit(1);
        }
        */

    //    // warmup map
    //    IMemMap imem_map_warmup;
    //    IMemMapVec imem_mapvec_warmup;
    //    imem_mapvec_warmup.push_back(imem_map_warmup);
    //    IMemMapVec::iterator itv_warmup = imem_mapvec_warmup.begin();
    //
    //    // runlength map
    //    IMemMap imem_map_runlen;
    //    IMemMapVec imem_mapvec_runlen;
    //    imem_mapvec_runlen.push_back(imem_map_runlen);
    //    IMemMapVec::iterator itv_runlen = imem_mapvec_runlen.begin();
    //
    //    // full map

        std::unique_ptr<IMemMapVec> imem_mapvec;
        if(config.java_trace) {
            imem_mapvec = std::make_unique<JavaIMem>();
        }
        else {
            imem_mapvec = std::make_unique<IMem>();
        }

        imem_mapvec->processTrace(config);
        imem_mapvec->print(config);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
