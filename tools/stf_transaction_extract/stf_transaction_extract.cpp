
// <STF_extract> -*- C++ -*-

/**
 * \brief  This tool extract transactions from an existing trace file
 *
 * Usage:
 * -b 1000:  head only the first 1000 transactions.
 * -s 1000:  skip the first 1000, output the rest.
 *
 */

#include <iostream>
#include <vector>

#include "command_line_parser.hpp"
#include "stf_transaction_extract.hpp"
#include "tools_util.hpp"

/**
 * \brief Parse the command line options
 *
 */
static STFTransactionExtractConfig parse_command_line (int argc, char **argv)
{
    STFTransactionExtractConfig config;
    trace_tools::CommandLineParser parser("stf_transaction_extract");
    parser.addFlag('s', "n", "skip the first n transactions");
    parser.addFlag('k', "n", "keep only the first n transactions");
    parser.addFlag('t', "n", "output trace files each containing n transactions");
    parser.addFlag('o', "trace", "output filename. stdout is default");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.appendHelpText("common usages:");
    //parser.appendHelpText("    -b <n> -e <m> -o <output> <input> -- write transactions [<n>, <m>) from <input> to <output>");
    parser.appendHelpText("    -s <n> -k <m> -o <output> <input> -- skip the first <n> transactions and write the next <m> to <output>");
    parser.appendHelpText("    -s <n> -o <output> <input> -- skip the first <n> transactions and write the rest to <output>");
    parser.appendHelpText("    -s <n> -t <m> <output> <input> -- skip the first <n> transactions and write every <m> transactions to <output.xxxxx.zstf>");
    parser.appendHelpText("    -t <m> -o <output> <input> -- write every <m> transactions to <output.xxxxx.zstf>");

    parser.setMutuallyExclusive('k', 't');

    parser.parseArguments(argc, argv);

    parser.getArgumentValue('s', config.skip_count);
    parser.getArgumentValue('k', config.head_count);
    parser.getArgumentValue('t', config.split_count);
    parser.getArgumentValue('o', config.output_filename);

    parser.getPositionalArgument(0, config.trace_filename);

    return config;
}

int main(int argc, char **argv)
{
    try {
        const STFTransactionExtractConfig config = parse_command_line (argc, argv);
        STFTransactionExtractor extractor(config);
        extractor.run(config.head_count, config.skip_count, config.split_count, config.output_filename);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
