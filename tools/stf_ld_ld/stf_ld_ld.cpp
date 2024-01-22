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
                        bool& verbose,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_ld_ld");
    parser.addFlag('w', "window_size", "window size (in # instructions)");
    parser.addFlag('v', "verbose output");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    parser.getArgumentValue('w', window_size);
    verbose = parser.hasArgument('v');
    parser.getPositionalArgument(0, trace);
}

int main(int argc, char** argv) {
    std::string trace;
    uint64_t window_size = std::numeric_limits<uint64_t>::max();
    bool verbose = false;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, window_size, verbose, skip_non_user);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    stf::STFInstReader reader(trace, skip_non_user);
    LdLdDependencyTracker tracker(window_size);
    std::map<uint64_t, uint64_t> distance_counts;
    std::map<uint64_t, std::map<uint64_t, uint64_t>> ld_ld_consumers;

    for(const auto& inst: reader) {
        if(STF_EXPECT_FALSE(inst.isLoad())) {
            const auto distances = tracker.getProducerDistances(inst);
            tracker.track(inst);
            if(!distances.empty()) {
                const auto max_it = distances.rbegin();
                ++distance_counts[max_it->first];
                if(verbose) {
                    ++ld_ld_consumers[max_it->first][inst.pc()];
                }
            }
        }
        else {
            tracker.track(inst);
        }
    }

    static constexpr int COLUMN_WIDTH = 20;
    stf::print_utils::printLeft("Distance", COLUMN_WIDTH);
    stf::print_utils::printLeft("Count", COLUMN_WIDTH);
    if(verbose) {
        stf::print_utils::printLeft("Consumer PCs", COLUMN_WIDTH);
    }
    std::cout << std::endl;
    for(const auto& p: distance_counts) {
        stf::print_utils::printDecLeft(p.first, COLUMN_WIDTH);
        stf::print_utils::printDecLeft(p.second, COLUMN_WIDTH);
        std::cout << std::endl;
        if(verbose) {
            for(const auto& consumer_pair: ld_ld_consumers[p.first]) {
                const auto pc = consumer_pair.first;
                const auto count = consumer_pair.second;
                stf::print_utils::printSpaces(COLUMN_WIDTH);
                stf::print_utils::printDecLeft(count, COLUMN_WIDTH);
                stf::print_utils::printHex(pc);
                std::cout << std::endl;
            }
        }
    }

    return 0;
}
