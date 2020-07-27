
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
    uint64_t repeat = 0;
    std::string output_filename = "-";
    FileList tracelist;

    trace_tools::CommandLineParser parser("stf_merge");

    parser.addMultiFlag('b', "N", "starting from the n instruction (inclusive) of input trace. Default is 1.");
    parser.addMultiFlag('e', "M", "ending at the nth instruction (exclusive) of input trace");
    parser.addMultiFlag('r', "K", "repeat the specified intruction interval [N, M) K times. Default is 0.");
    parser.addMultiFlag('f', "trace", "filename of input trace");
    parser.addMultiFlag('o', "trace", "output trace filename. stdout is default");
    parser.appendHelpText("-b, -e, -r, and -f flags can be specified multiple times for merging multiple traces");

    parser.parseArguments(argc, argv);

    if(STF_EXPECT_FALSE(!parser.hasArgument('f'))) {
        parser.raiseErrorWithHelp("Must specify at least 1 input trace.");
    }

    parser.getArgumentValue('o', output_filename);

    auto b_it = parser.getMultipleValueArgument('b').begin();
    auto e_it = parser.getMultipleValueArgument('e').begin();
    auto r_it = parser.getMultipleValueArgument('r').begin();
    auto f_it = parser.getMultipleValueArgument('f').begin();
    for(const auto& arg: parser) {
        switch(arg->first) {
            case 'b':
                bcount = parseInt<uint64_t>(*b_it);
                ++b_it;
                break;
            case 'e':
                ecount = parseInt<uint64_t>(*e_it);
                ++e_it;
                break;
            case 'r':
                repeat = parseInt<uint64_t>(*r_it);
                ++r_it;
                break;
            case 'f':
                stf_assert(ecount > bcount, "End count must be greater than start count");
                tracelist.emplace_back(bcount, ecount, repeat, *f_it);
                ++f_it;

                // clear it for next trace input;
                bcount = 1;
                ecount = std::numeric_limits<uint64_t>::max();
                repeat = 0;
                break;
        }
    }

    return std::make_pair(tracelist, output_filename);
}

/**
 * \brief process merge request from same intput file;
 * return 0 on success; otherwise -1;
 */
int processSameFile(stf::STFWriter& writer, const FileList& tracelist) {
    int retcode = 0;

    stf::STF_PTE page_table(nullptr, nullptr, true);
    auto fit = tracelist.begin();
    // Open stf trace reader
    stf::STFReader stf_reader(fit->filename);
    if (!stf_reader) {
        return -1;
    }

    // TODO: check overlap here;
    uint64_t totalcount = 0;
    STFMergeExtractor extractor;
    for (fit = tracelist.begin(); fit != tracelist.end(); ++fit) {
        if (fit->start -1 < totalcount) {
            std::cerr << "Error 1: required to skip to "
                      << fit->start - 1
                      << ", actually skip to "
                      << totalcount
                      << std::endl;
            retcode = -1;
            break;
        }
        uint64_t skipCount = fit->start - totalcount -1;
        totalcount += extractor.extractSkip(stf_reader, skipCount, page_table);

        if (totalcount != fit->start - 1) {
            std::cerr << "Error 2: required to skip to "
                      << fit->start - 1
                      << ", actually skip to "
                      << totalcount
                      << std::endl;
            retcode = -1;
            break;
        }

        totalcount += extractor.extractInsts(stf_reader, writer, (fit->end - fit->start), page_table);
    }

    stf_reader.close();

    return retcode;
}

int processDiffFiles(stf::STFWriter& writer, const FileList& tracelist) {
    stf::STF_PTE page_table(nullptr, nullptr, true);
    stf::STFReader stf_reader;
    STFMergeExtractor extractor;
    for (const auto& f: tracelist) {
        stf_reader.open(f.filename);
        // Open stf trace reader
        for (uint64_t i = 0; i < f.repeat + 1; i++) {
            if (!stf_reader) {
                break;
            }

            if (extractor.extractSkip(stf_reader, f.start - 1, page_table) == f.start - 1) {
                extractor.extractInsts(stf_reader, writer, (f.end - f.start), page_table);
            }
            stf_reader.close();
        }
    }

    return 0;
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

    bool sameInput = true;
    std::string inputfilename = tracelist.begin()->filename;
    uint64_t repeat = tracelist.begin()->repeat;
    for (const auto& f: tracelist) {
        if (inputfilename != f.filename || repeat != f.repeat) {
            sameInput = false;
            break;
        }
        inputfilename = f.filename;
        repeat = f.repeat;
    }

    // open output file to write;
    stf::STFWriter writer(output_filename);
    if (!writer) {
        return -1;
    }

    if (sameInput) {
        processSameFile(writer, tracelist);
    }
    else {
        processDiffFiles(writer, tracelist);
    }

    writer.close();
    return 0;
}
