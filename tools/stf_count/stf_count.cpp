
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
                               bool& short_output,
                               bool& csv_output,
                               bool& cumulative_csv,
                               uint64_t& csv_interval,
                               uint64_t& start_inst,
                               uint64_t& end_inst) {
    trace_tools::CommandLineParser parser("stf_count");
    parser.addFlag('v', "multi-line output");
    parser.addFlag('u', "only count user-mode instructions. When this is enabled, -i/-s/-e parameters will be in terms of user-mode instructions.");
    parser.addFlag('S', "short output - only output instruction count");
    parser.addFlag('c', "output in csv format");
    parser.addFlag('C', "make CSV output cumulative");
    parser.addFlag('i', "interval", "dump CSV on this instruction interval");
    parser.addFlag('s', "N", "start counting at Nth instruction");
    parser.addFlag('e', "M", "stop counting at Mth instruction");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    parser.getArgumentValue('v', verbose);
    parser.getArgumentValue('u', user_mode_only);
    parser.getArgumentValue('S', short_output);
    parser.getArgumentValue('c', csv_output);
    parser.getArgumentValue('C', cumulative_csv);
    parser.getArgumentValue('i', csv_interval);
    parser.getArgumentValue('s', start_inst);
    parser.getArgumentValue('e', end_inst);

    if(short_output && verbose) {
        parser.raiseErrorWithHelp("Specifying -s and -v at the same time is not allowed.");
    }

    if(short_output && csv_output) {
        parser.raiseErrorWithHelp("Specifying -s and -c at the same time is not allowed.");
    }

    if(verbose && csv_output) {
        parser.raiseErrorWithHelp("Specifying -c and -v at the same time is not allowed.");
    }

    if(!csv_output && cumulative_csv) {
        parser.raiseErrorWithHelp("The -C flag is only valid when -c is specified");
    }

    if(!csv_interval) {
        cumulative_csv = true; // If we aren't doing interval dumps, the CSV should always be cumulative
    }

    parser.getPositionalArgument(0, trace_filename);
}

int main (int argc, char **argv) {
    std::string trace_filename;
    bool verbose = false;
    bool user_mode_only = false;
    bool short_output = false;
    bool csv_output = false;
    bool cumulative_csv = false;
    uint64_t csv_interval = 0;
    uint64_t start_inst = 0;
    uint64_t end_inst = std::numeric_limits<uint64_t>::max();

    try {
        parse_command_line(argc,
                           argv,
                           trace_filename,
                           verbose,
                           user_mode_only,
                           short_output,
                           csv_output,
                           cumulative_csv,
                           csv_interval,
                           start_inst,
                           end_inst);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    stf::STFInstReader stf_inst_reader(trace_filename, user_mode_only);

    STFCountFilter stf_count_filter(stf_inst_reader,
                                    verbose,
                                    short_output,
                                    user_mode_only,
                                    csv_output,
                                    cumulative_csv,
                                    csv_interval);

    stf_count_filter.extract(start_inst, end_inst - start_inst, stf_inst_reader.getISA(), stf_inst_reader.getInitialIEM());

    return 0;
}
