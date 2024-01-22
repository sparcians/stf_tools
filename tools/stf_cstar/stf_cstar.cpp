#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "stf_gzip_bin.hpp"
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
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_cstar");
    parser.addFlag('e', "elf", "ELF file to search for always-skipped instructions");
    parser.addFlag('b', "bin", "Binary file to search for always-skipped instructions");
    parser.addFlag('o', "offset", "VA offset for raw binary files. Defaults to 0x80000000.");
    parser.addFlag('u', "skip non user-mode instructions");

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

    parser.getPositionalArgument(0, trace);
}

struct CStarInfo {
    CStarInfo(const uint32_t _branch_opcode,
              const uint64_t _next_inst_pc,
              const uint32_t _next_inst_opcode) :
        next_inst_pc(_next_inst_pc),
        next_inst_opcode(_next_inst_opcode),
        branch_opcode(_branch_opcode)
    {
    }

    uint64_t next_inst_pc;
    uint32_t next_inst_opcode;
    uint32_t branch_opcode;
};

struct BranchInfo {
    uint64_t taken;
    uint64_t not_taken;
    uint64_t num_toggle;
    bool last_taken;
};

using BranchMap = std::map<uint64_t, BranchInfo>;

template<bool taken>
inline void updateBranchInfo(BranchMap& branch_counts, const uint64_t pc) {
    auto[it, new_branch] = branch_counts.try_emplace(pc);
    auto& branch_info = it->second;

    bool toggle = new_branch;

    if(taken) {
        ++branch_info.taken;
        toggle |= !branch_info.last_taken;
    }
    else {
        ++branch_info.not_taken;
        toggle |= branch_info.last_taken;
    }
    branch_info.num_toggle += toggle;
    branch_info.last_taken = taken;
}

int main(int argc, char** argv) {
    std::string trace;
    std::string binary_file;
    uint64_t binary_offset = 0;
    bool elf = false;
    bool bin = false;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, binary_file, binary_offset, elf, bin, skip_non_user);
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
        if(binary_file.rfind(".gz") == binary_file.size() - 3) {
            binary = std::make_unique<STFGzipBinary>(binary_offset);
        }
        else {
            binary = std::make_unique<STFBinary>(binary_offset);
        }
    }

    if(binary) {
        binary->open(binary_file);
    }

    stf::STFInstReader reader(trace, skip_non_user);

    stf::Disassembler dis(findElfFromTrace(trace), reader.getISA(), reader.getInitialIEM(), false);

    bool last_was_nt_branch = false;
    uint64_t last_branch_target = 0;
    uint64_t last_branch_pc = 0;
    uint32_t last_branch_opcode = 0;
    stf::STFDecoder decoder(reader.getInitialIEM());
    BranchMap branch_counts;
    std::map<uint64_t, CStarInfo> cstar_info;

    // First find all instances of a not-taken conditional branch followed by another instruction
    for(const auto& inst: reader) {
        const auto inst_pc = inst.pc();
        const auto inst_opcode = inst.opcode();
        const auto next_pc = inst_pc + inst.opcodeSize();

        // If it's a taken branch, we won't be able to tell what the next sequential instruction is.
        // So, we count it and check later if it was an actual cstar candidate
        if(STF_EXPECT_FALSE(inst.isTakenBranch())) {
            last_was_nt_branch = false;
            updateBranchInfo<true>(branch_counts, inst_pc);

            if(binary) {
                decoder.decode(inst_opcode);
                if(!decoder.isInstType(mavis::InstMetaData::InstructionTypes::CONDITIONAL)) {
                    continue;
                }

                if(inst.branchTarget() > next_pc) {
                    const auto branch_distance = (inst.branchTarget() - next_pc);

                    uint32_t next_opcode = 0;
                    if(branch_distance == 2) {
                        next_opcode = binary->read<uint16_t>(next_pc);
                    }
                    else if(branch_distance == 4) {
                        next_opcode = binary->read<uint32_t>(next_pc);
                        // Make sure the next instruction isn't compressed
                        // If it is, then this branch is skipping over more than 1 instruction
                        if(stf::STFDecoder::isCompressed(next_opcode)) {
                            continue;
                        }
                    }

                    if(next_opcode) {
                        decoder.decode(next_opcode);
                        if(STF_EXPECT_TRUE(!decoder.isBranch())) {
                            cstar_info.try_emplace(inst_pc,
                                                   inst_opcode, next_pc, next_opcode);
                        }
                    }
                }
            }
            continue;
        }

        decoder.decode(inst_opcode);

        if(STF_EXPECT_FALSE(decoder.isBranch() && decoder.isConditional())) {
            last_was_nt_branch = true;
            updateBranchInfo<false>(branch_counts, inst_pc);

            // Compute the target
            last_branch_pc = inst_pc;
            last_branch_target = last_branch_pc + static_cast<uint64_t>(decoder.getSignedImmediate());
            last_branch_opcode = inst_opcode;
            continue;
        }

        if(STF_EXPECT_FALSE(last_was_nt_branch && !decoder.isBranch())) {
            if(last_branch_target == next_pc) {
                cstar_info.try_emplace(last_branch_pc,
                                       last_branch_opcode, inst_pc, inst_opcode);
            }
        }

        last_was_nt_branch = false;
    }

    std::map<std::string, std::map<std::string, std::tuple<uint64_t, uint64_t, uint64_t>>> mnemonic_counts;

    for(const auto& p: cstar_info) {
        const auto& [branch_pc, cstar] = p;
        auto& [taken, not_taken, toggle] = mnemonic_counts[decoder.decode(cstar.branch_opcode).getMnemonic()][decoder.decode(cstar.next_inst_opcode).getMnemonic()];
        const auto& branch_info = branch_counts[branch_pc];
        taken += branch_info.taken;
        not_taken += branch_info.not_taken;
        toggle += branch_info.num_toggle;
    }

    std::cout << "Branch Mnemonic,Next Inst Mnemonic,Taken Count,Not Taken Count,Toggle Count" << std::endl;
    for(const auto& p: mnemonic_counts) {
        const auto& [branch_mnemonic, next_inst_info] = p;
        for(const auto& q: next_inst_info) {
            const auto& next_inst_mnemonic = q.first;
            const auto& [taken, not_taken, toggle] = q.second;
            std::cout << branch_mnemonic << ',' << next_inst_mnemonic << ',' << taken << ',' << not_taken << ',' << toggle << std::endl;
        }
    }

    return 0;
}
