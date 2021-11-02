#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "print_utils.hpp"
#include "stf_branch_reader.hpp"
#include "command_line_parser.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_branch_dump");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    skip_non_user = parser.hasArgument('u');

    parser.getPositionalArgument(0, trace);
}

int main(int argc, char** argv) {
    std::string trace;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, skip_non_user);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    stf::STFBranchReader reader(trace, skip_non_user);

    for(const auto& branch: reader) {
        std::cout << branch << std::endl;
    }

    return 0;
}
