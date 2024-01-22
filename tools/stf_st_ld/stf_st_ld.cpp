#include <iostream>
#include <string>

#include "print_utils.hpp"
#include "stf_inst_reader.hpp"
#include "command_line_parser.hpp"
#include "dependency_tracker.hpp"
#include "tools_util.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        uint64_t& window_size,
                        uint64_t& alignment,
                        bool& verbose,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_st_ld");
    parser.addFlag('w', "window_size", "window size (in # instructions)");
    parser.addFlag('v', "verbose output");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('a', "alignment", "align addresses to the specified number of bytes");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    parser.getArgumentValue('w', window_size);
    verbose = parser.hasArgument('v');
    skip_non_user = parser.hasArgument('u');
    parser.getArgumentValue('a', alignment);
    parser.getPositionalArgument(0, trace);
}

int main(int argc, char** argv) {
    std::string trace;
    uint64_t window_size = std::numeric_limits<uint64_t>::max();
    uint64_t alignment = 1;
    bool verbose = false;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, window_size, alignment, verbose, skip_non_user);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    const uint64_t address_mask = std::numeric_limits<uint64_t>::max() << log2(alignment);

    stf::STFInstReader reader(trace, skip_non_user);
    StLdDependencyTracker tracker(window_size, address_mask);
    std::map<uint64_t, uint64_t> distance_counts;
    std::map<uint64_t, std::map<uint64_t, uint64_t>> address_counts;

    for(const auto& inst: reader) {
        const auto distances = tracker.getProducerDistances(inst);
        tracker.track(inst);
        if(!distances.empty()) {
            const auto max_it = distances.rbegin();
            ++distance_counts[max_it->first];
            ++address_counts[max_it->first][max_it->second];
        }
    }

    static constexpr int COLUMN_WIDTH = 20;
    stf::print_utils::printLeft("Distance", COLUMN_WIDTH);
    stf::print_utils::printLeft("Count", COLUMN_WIDTH);
    if(verbose) {
        std::cout << "Address";
    }
    std::cout << std::endl;
    for(const auto& p: distance_counts) {
        stf::print_utils::printDecLeft(p.first, COLUMN_WIDTH);
        stf::print_utils::printDecLeft(p.second, COLUMN_WIDTH);
        std::cout << std::endl;
        if(verbose) {
            for(const auto& addr_pair: address_counts[p.first]) {
                stf::print_utils::printSpaces(COLUMN_WIDTH);
                stf::print_utils::printDecLeft(addr_pair.second, COLUMN_WIDTH);
                stf::print_utils::printHex(addr_pair.first);
                std::cout << std::endl;
            }
        }
    }

    return 0;
}
