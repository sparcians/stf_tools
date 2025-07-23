#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "stf_elf.hpp"
#include "stf_decoder.hpp"
#include "disassembler.hpp"

#include "print_utils.hpp"
#include "stf_inst_reader.hpp"
#include "command_line_parser.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        std::string& binary_file,
                        uint64_t& binary_offset,
                        bool& elf,
                        bool& bin,
                        bool& only_dynamic,
                        bool& skip_non_user,
                        bool& show_disasm) {
    trace_tools::CommandLineParser parser("stf_cmov");
    parser.addFlag('e', "elf", "ELF file to search for always-skipped move instructions");
    parser.addFlag('b', "bin", "Binary file to search for always-skipped move instructions");
    parser.addFlag('o', "offset", "VA offset for raw binary files. Defaults to 0x80000000.");
    parser.addFlag('d', "only report dynamic branches (branches that are not always-taken or never-taken)");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('D', "show disassembly of branch/mov pairs");

    parser.setMutuallyExclusive('e', 'b');
    parser.setMutuallyExclusive('e', 'o', "-o is only applicable to flat binary files.");
    parser.setDependentArgument('o', 'b', "-o option has no effect without -b");

    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    skip_non_user = parser.hasArgument('u');
    elf = parser.hasArgument('e');
    bin = parser.hasArgument('b');

    if(elf) {
        parser.getArgumentValue('e', binary_file);
    }
    else if(bin) {
        parser.getArgumentValue('b', binary_file);
        binary_offset = 0x80000000;
        parser.getArgumentValue('o', binary_offset);
    }

    only_dynamic = parser.hasArgument('d');
    show_disasm = parser.hasArgument('D');

    parser.getPositionalArgument(0, trace);
}

struct CMovInfo {
    uint32_t branch_opcode;
    uint64_t mov_pc;
    uint32_t mov_opcode;
};

template<int COLUMN_WIDTH, int COLUMN_INDEX>
void printDisasmLine(const stf::Disassembler& dis, const uint64_t pc, const uint32_t opcode) {
    static_assert(COLUMN_WIDTH >= 16, "Column width must be >= 16");
    stf::print_utils::printSpaces(COLUMN_INDEX * COLUMN_WIDTH);
    stf::print_utils::printHex(pc);
    stf::print_utils::printSpaces(COLUMN_WIDTH - 16);
    dis.printDisassembly(std::cout, pc, opcode);
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    std::string trace;
    std::string binary_file;
    uint64_t binary_offset = 0;
    bool elf = false;
    bool bin = false;
    bool only_dynamic = false;
    bool skip_non_user = false;
    bool show_disasm = false;

    try {
        processCommandLine(argc, argv, trace, binary_file, binary_offset, elf, bin, only_dynamic, skip_non_user, show_disasm);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    std::unique_ptr<STFBinary> binary;

    if(elf) {
        binary = std::make_unique<STFElf>();
    }
    else if(bin) {
        binary = std::make_unique<STFBinary>(binary_offset);
    }

    if(binary) {
        binary->open(binary_file);
    }

    stf::STFInstReader reader(trace, skip_non_user);

    stf::Disassembler dis(findElfFromTrace(trace), reader, false);

    bool last_was_nt_branch = false;
    uint64_t last_branch_target = 0;
    uint64_t last_branch_pc = 0;
    uint32_t last_branch_opcode = 0;
    stf::STFDecoder decoder(reader);
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> branch_counts;
    std::set<uint64_t> cmov_branches;
    std::map<uint64_t, CMovInfo> cmov_info;

    // First find all instances of a not-taken conditional branch followed by a move
    for(const auto& inst: reader) {
        // If it's a taken branch, we won't be able to tell what the next sequential instruction is.
        // So, we count it and check later if it was an actual cmov candidate
        if(STF_EXPECT_FALSE(inst.isTakenBranch())) {
            last_was_nt_branch = false;
            ++branch_counts[inst.pc()].first;
            if(binary) {
                decoder.decode(inst.opcode());
                if(!decoder.isInstType(mavis::InstMetaData::InstructionTypes::CONDITIONAL)) {
                    continue;
                }

                const auto next_pc = inst.pc() + inst.opcodeSize();
                if(inst.branchTarget() > next_pc) {
                    const auto branch_distance = (inst.branchTarget() - next_pc);

                    uint32_t opcode = 0;
                    if(branch_distance == 2) {
                        opcode = binary->read<uint16_t>(next_pc);
                    }
                    else if(branch_distance == 4) {
                        opcode = binary->read<uint32_t>(next_pc);
                        // Make sure the next instruction isn't compressed
                        // If it is, then this branch is skipping over more than 1 instruction
                        if(stf::STFDecoder::isCompressed(opcode)) {
                            continue;
                        }
                    }

                    if(opcode) {
                        decoder.decode(opcode);
                        if(decoder.isInstType(mavis::InstMetaData::InstructionTypes::MOVE) &&
                           !decoder.isInstType(mavis::InstMetaData::InstructionTypes::FLOAT)) {
                            cmov_branches.insert(inst.pc());
                            if(show_disasm) {
                                cmov_info.emplace(inst.pc(), CMovInfo{inst.opcode(), next_pc, opcode});
                            }
                        }
                    }
                }
            }
            continue;
        }

        // We can avoid decoding the instruction if we know for a fact it isn't a branch or a move
        if(STF_EXPECT_FALSE(inst.isLoad() || inst.isStore() || inst.isSyscall() || inst.isFP())) {
            last_was_nt_branch = false;
            continue;
        }

        decoder.decode(inst.opcode());

        if(STF_EXPECT_FALSE(decoder.isBranch() && decoder.isConditional())) {
            last_was_nt_branch = true;
            // Compute the target
            ++branch_counts[inst.pc()].second;
            last_branch_target = inst.pc() + static_cast<uint64_t>(decoder.getSignedImmediate());
            last_branch_pc = inst.pc();
            last_branch_opcode = inst.opcode();
            continue;
        }

        // Only detect integer move instructions
        if(STF_EXPECT_FALSE(last_was_nt_branch &&
                            decoder.isInstType(mavis::InstMetaData::InstructionTypes::MOVE) &&
                            !decoder.isInstType(mavis::InstMetaData::InstructionTypes::FLOAT))) {
            const auto next_pc = inst.pc() + inst.opcodeSize();
            if(last_branch_target == next_pc) {
                cmov_branches.insert(last_branch_pc);
                if(show_disasm) {
                    cmov_info.emplace(last_branch_pc, CMovInfo{last_branch_opcode, inst.pc(), inst.opcode()});
                }
            }
        }

        last_was_nt_branch = false;
    }

    static constexpr int COLUMN_WIDTH = 20;
    stf::print_utils::printLeft("Start PC", COLUMN_WIDTH);
    stf::print_utils::printLeft("Total Instances", COLUMN_WIDTH);
    stf::print_utils::printLeft("Taken Count", COLUMN_WIDTH);
    stf::print_utils::printLeft("Not Taken Count", COLUMN_WIDTH);
    if(show_disasm) {
        stf::print_utils::printLeft("Instruction PC", COLUMN_WIDTH);
        stf::print_utils::printLeft("Disassembly", COLUMN_WIDTH);
    }
    std::cout << std::endl;
    for(const auto pc: cmov_branches) {
        const auto& counts = branch_counts[pc];
        const auto taken = counts.first;
        const auto not_taken = counts.second;
        if(only_dynamic && !(taken && not_taken)) {
            continue;
        }
        const auto total = taken + not_taken;
        stf::print_utils::printHex(pc);
        stf::print_utils::printSpaces(4);
        stf::print_utils::printDecLeft(total, COLUMN_WIDTH);
        stf::print_utils::printDecLeft(taken, COLUMN_WIDTH);
        stf::print_utils::printDecLeft(not_taken, COLUMN_WIDTH);
        std::cout << std::endl;
        if(show_disasm) {
            const auto& cmov = cmov_info[pc];

            printDisasmLine<COLUMN_WIDTH, 4>(dis, pc, cmov.branch_opcode);
            printDisasmLine<COLUMN_WIDTH, 4>(dis, cmov.mov_pc, cmov.mov_opcode);
        }
    }

    return 0;
}
