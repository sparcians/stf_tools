
// <STF_count> -*- C++ -*-

/**
 * \brief  This file counts the number of records, instructions, and memory accesses
 *
 */

#include <iostream>
#include <locale>
#include <iomanip>
#include <limits>
#include <string>

#include "command_line_parser.hpp"
#include "stf_count.hpp"
#include "stf_writer.hpp"
#include "tools_util.hpp"

/**
 * \brief Parse the command line options
 *
 */
static void parse_command_line(int argc,
                               char **argv,
                               std::string& trace_filename,
                               bool& verbose,
                               bool& user_mode_only,
                               bool& short_output) {
    trace_tools::CommandLineParser parser("stf_count");
    parser.addFlag('v', "multi-line output");
    parser.addFlag('u', "only count user-mode instructions");
    parser.addFlag('s', "short output - only output instruction count");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    parser.getArgumentValue('v', verbose);
    parser.getArgumentValue('u', user_mode_only);
    parser.getArgumentValue('s', short_output);

    if(short_output && verbose) {
        parser.raiseErrorWithHelp("Specifying -s and -v at the same time is not allowed.");
    }

    parser.getPositionalArgument(0, trace_filename);
}

int main (int argc, char **argv) {
    std::string trace_filename;
    bool verbose = false;
    bool user_mode_only = false;
    bool short_output = false;

    try {
        parse_command_line (argc, argv, trace_filename, verbose, user_mode_only, short_output);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    stf::STFInstReader stf_inst_reader(trace_filename, user_mode_only);

    STFCountFilter stf_count_filter(stf_inst_reader, verbose, short_output);

    stf_count_filter.extract(0, std::numeric_limits<uint64_t>::max(), stf_inst_reader.getISA(), stf_inst_reader.getInitialIEM());

    return 0;
}
