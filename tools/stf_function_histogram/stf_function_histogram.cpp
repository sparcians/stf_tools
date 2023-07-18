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
        const size_t ext_idx = trace.rfind(".zstf");
        elf = trace.substr(0, ext_idx) + ".elf";
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
        const auto iem = inst_reader.getInitialIEM();
        stf::STFBranch branch;
        stf::STFBranchDecoder branch_decoder;
        // Iterate 1 instruction at a time until we hit one that falls within the provided ELF
        for(const auto& inst: inst_reader) {
            if(const auto pc = inst.pc(); hist.validPC(pc)) {
                // Branches will be handled by the
                if(STF_EXPECT_FALSE(inst.isBranch())) {
                    const auto& orig_records = inst.getOrigRecords();
                    if(inst.opcodeSize() == sizeof(uint16_t)) {
                        branch_decoder.decode(iem, orig_records.at(stf::descriptors::internal::Descriptor::STF_INST_OPCODE16)[0]->as<stf::InstOpcode16Record>(), branch);
                    }
                    else {
                        branch_decoder.decode(iem, orig_records.at(stf::descriptors::internal::Descriptor::STF_INST_OPCODE32)[0]->as<stf::InstOpcode32Record>(), branch);
                    }
                    if(STF_EXPECT_FALSE(branch.isCall())) {
                        break;
                    }
                }
                hist.count(pc);
                break;
            }
        }
    }

    stf::STFBranchReader reader(trace, skip_non_user);

    for(const auto& branch: reader) {
        if(STF_EXPECT_FALSE(branch.getInstIndex() >= end_insts)) {
            break;
        }

        if(STF_EXPECT_FALSE(branch.isCall())) {
            hist.count(branch.getTargetPC());
        }
    }

    hist.dump();

    return 0;
}
