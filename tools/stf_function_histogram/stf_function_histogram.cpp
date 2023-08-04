#include "stf_branch_reader.hpp"
#include "stf_inst_reader.hpp"

#include "stf_function_histogram.hpp"
#include "command_line_parser.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        std::string& elf,
                        bool& skip_non_user,
                        uint64_t& end_insts) {
    trace_tools::CommandLineParser parser("stf_function_histogram");
    parser.addFlag('E', "elf", "ELF file to analyze (defaults to trace.elf)");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('e', "end_insts", "stop after specified number of instructions");

    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    skip_non_user = parser.hasArgument('u');

    end_insts = std::numeric_limits<uint64_t>::max();
    parser.getArgumentValue('e', end_insts);

    parser.getPositionalArgument(0, trace);

    if(parser.hasArgument('E')) {
        parser.getArgumentValue('E', elf);
    }
    else {
        elf = findElfFromTrace(trace);
    }
}

int main(int argc, char** argv) {
    std::string trace;
    std::string elf;
    bool skip_non_user;
    uint64_t end_insts;

    try {
        processCommandLine(argc, argv, trace, elf, skip_non_user, end_insts);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    SymbolHistogram hist(elf);

    {
        // Find out where the trace starts
        stf::STFInstReader inst_reader(trace, skip_non_user);
        for(const auto& inst: inst_reader) {
            if(STF_EXPECT_FALSE(inst.index() >= end_insts)) {
                break;
            }
            hist.count(inst.pc());
        }
    }

    hist.dump();

    return 0;
}
