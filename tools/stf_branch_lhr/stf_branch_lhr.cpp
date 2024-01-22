#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "stf_decoder.hpp"

#include "print_utils.hpp"
#include "stf_branch_reader.hpp"
#include "command_line_parser.hpp"
#include <boost/multiprecision/cpp_int.hpp>

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        bool& verbose,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_branch_lhr");
    parser.appendHelpText("Detect and output the local history of all static branches in the specified STF");
    parser.addFlag('v', "verbose mode (prints indirect branch targets)");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    verbose = parser.hasArgument('v');
    skip_non_user = parser.hasArgument('u');

    parser.getPositionalArgument(0, trace);
}


struct BranchInfo {
    static constexpr uint32_t MAX_LHR = 1000;
    uint32_t count = 0;
    std::bitset<MAX_LHR> lhr;
};


// This program detects and outputs local history of all static branches in the specified trace
int main(int argc, char** argv) {
    std::string trace;
    bool verbose = false;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, verbose, skip_non_user);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    stf::STFBranchReader reader(trace, skip_non_user);

    std::map<uint64_t, BranchInfo> branches;

    // Determine the local history of all conditional branches in trace
    for(const auto& branch: reader) {
        if (branch.isConditional()) {
            const auto pc = branch.getPC();
            const bool is_taken = branch.isTaken();
            auto& branch_info = branches[pc];
            ++branch_info.count;
            branch_info.lhr <<= 1;
            if(is_taken) {
                branch_info.lhr |= 0x1;
            }
        }
    }

    std::cout << std::setw(10) << "PC  " << std::setw(7) << "br_count " << " LHR" << std::endl;
    std::cout << std::endl;
    for(const auto& branch: branches) {
        const auto& branch_info = branch.second;
        const auto pc = branch.first;
        std::cout << std::hex << std::setw(10) << pc
                  << " " << std::dec << std::setw(7) << branch_info.count
                  << " " << branch_info.lhr.to_string()
                  << std::endl;
    }

    return 0;
}
