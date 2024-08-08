
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
#include <limits>
#include <map>
#include <vector>

#include "command_line_parser.hpp"
#include "stf_merge.hpp"
#include "tools_util.hpp"

/**
 * \brief Parse the command line options
 *
 */
static std::pair<FileList, std::string> parseCommandLine (int argc, char **argv) {
    // Parse options
    uint64_t bcount = 1;
    uint64_t ecount = std::numeric_limits<uint64_t>::max();
    uint64_t repeat = 1;
    std::string output_filename = "-";
    FileList tracelist;

    trace_tools::CommandLineParser parser("stf_merge");

    parser.addMultiFlag('b', "N", "starting from the n instruction (inclusive) of input trace. Default is 1.");
    parser.addMultiFlag('e', "M", "ending at the nth instruction (exclusive) of input trace");
    parser.addMultiFlag('r', "K", "repeat the specified intruction interval [N, M) K times. Default is 1.");
    parser.addMultiFlag('f', "trace", "filename of input trace", true, "Must specify at least 1 input trace.");
    parser.addFlag('o', "trace", "output trace filename. stdout is default");
    parser.appendHelpText("-b, -e, -r, and -f flags can be specified multiple times for merging multiple traces");

    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);

    for(const auto& arg: parser) {
        switch(arg.getFlag()) {
            case 'b':
                bcount = parseInt<uint64_t>(arg.getValue());
                parser.assertCondition(bcount >= 1, "Start count must be at least 1.");
                break;
            case 'e':
                ecount = parseInt<uint64_t>(arg.getValue());
                parser.assertCondition(ecount >= 2, "End count must be at least 2.");
                break;
            case 'r':
                repeat = parseInt<uint64_t>(arg.getValue());
                break;
            case 'f':
                parser.assertCondition(ecount > bcount, "End count must be greater than start count");
                tracelist.emplace_back(bcount, ecount, repeat, arg.getValue());

                // clear it for next trace input;
                bcount = 1;
                ecount = std::numeric_limits<uint64_t>::max();
                repeat = 1;
                break;
        }
    }

    return std::make_pair(tracelist, output_filename);
}

void processFiles(stf::STFWriter& writer, const FileList& tracelist) {
    stf::STF_PTE page_table(nullptr, nullptr, true);
    stf::STFReader reader;
    STFMergeExtractor extractor;
    bool reopen_trace = true;
    auto last_it = tracelist.begin();

    for (auto it = tracelist.begin(); it != tracelist.end(); ++it) {
        const auto& f = *it;
        uint64_t num_to_skip = f.start - 1;
        const uint64_t num_to_extract = f.end - f.start;

        if(!reopen_trace) {
            if(f.filename != last_it->filename || f.start < last_it->end) {
                reopen_trace = true;
            }
            else {
                num_to_skip = f.start - last_it->end;
            }
        }

        for(uint64_t i = 0; i < f.repeat; ++i) {
            if(reopen_trace || i > 0) {
                reader.close();
                reader.open(f.filename);
            }

            stf_assert(reader, "Failed to open input trace " << f.filename);

            stf_assert(extractor.extractSkip(reader, num_to_skip, page_table) == num_to_skip,
                       "Tried to skip past the end of the trace.");

            stf_assert(extractor.extractInsts(reader, writer, num_to_extract, page_table) == num_to_extract ||
                       num_to_extract == std::numeric_limits<uint64_t>::max() - 1,
                       "Tried to extract past the end of the trace.");
        }

        last_it = it;
        reopen_trace = false;
    }
}

int main(int argc, char **argv) {
    FileList tracelist;
    std::string output_filename;

    try {
        std::tie(tracelist, output_filename) = parseCommandLine (argc, argv);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    // open output file to write;
    stf::STFWriter writer(output_filename);
    stf_assert(writer, "Failed to open output trace.");

    processFiles(writer, tracelist);

    writer.close();

    return 0;
}
