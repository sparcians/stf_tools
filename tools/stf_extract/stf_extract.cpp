
// <STF_extract> -*- C++ -*-

/**
 * \brief  This tool extract records/instructions/memory accesses from an existing trace file
 *
 * Usage:
 * -b 1000:  head only the first 1000 records.
 * -s 1000:  skip the first 1000, output the rest.
 *
 */

#include <iostream>
#include <vector>

#include "command_line_parser.hpp"
#include "stf_extract.hpp"
#include "tools_util.hpp"

/**
 * \brief Parse the command line options
 *
 */
static STFExtractConfig parse_command_line (int argc, char **argv)
{
    STFExtractConfig config;
    trace_tools::CommandLineParser parser("stf_extract");
    parser.addFlag('s', "n", "skip the first n instructions");
    parser.addFlag('k', "n", "keep only the first n instructions");
    parser.addFlag('t', "n", "output trace files each containing n instructions");
    parser.addFlag('o', "trace", "output filename. stdout is default");
    parser.addFlag('f', "off", "offset by which to shift inst_pc");
    parser.addFlag('l', "filter out kernel code while extracting");
    parser.addFlag('d', "for slicing (-s/-k/-t) output PTE records on demand");
    parser.addFlag('u', "only count user-mode instructions for -s/-k/-t parameters. Non-user instructions will still be included in the extracted trace unless -l is specified.");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.appendHelpText("common usages:");
    //parser.appendHelpText("    -b <n> -e <m> -o <output> <input> -- write instructions [<n>, <m>) from <input> to <output>");
    parser.appendHelpText("    -s <n> -k <m> -o <output> <input> -- skip the first <n> instructions and write the next <m> to <output>");
    parser.appendHelpText("    -s <n> -o <output> <input> -- skip the first <n> instructions and write the rest to <output>");
    parser.appendHelpText("    -s <n> -t <m> <output> <input> -- skip the first <n> instructions and write every <m> instructions to <output.xxxxx.inst.stf.xz>");
    parser.appendHelpText("    -t <m> -o <output> <input> -- write every <m> instructions to <output.xxxxx.inst.stf.xz>");

    parser.setMutuallyExclusive('k', 't');

    parser.parseArguments(argc, argv);

    parser.getArgumentValue('s', config.skip_count);
    parser.getArgumentValue('k', config.head_count);
    parser.getArgumentValue('t', config.split_count);
    parser.getArgumentValue('o', config.output_filename);
    const bool has_f = parser.getArgumentValue('f', config.inst_offset);
    if(has_f && (config.inst_offset % 4)) {
        throw trace_tools::CommandLineParser::EarlyExitException(1, "Instruction PC offset must be a multiple of 4.");
    }
    config.filter_kernel_code = parser.hasArgument('l');
    config.dump_ptes_on_demand = parser.hasArgument('d');

    config.user_mode_counts = parser.hasArgument('u');

    parser.getPositionalArgument(0, config.trace_filename);

    return config;
}

int main(int argc, char **argv)
{
    try {
        const STFExtractConfig config = parse_command_line (argc, argv);
        STFExtractor extractor(config);
        extractor.run(config.head_count, config.skip_count, config.split_count, config.output_filename);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
