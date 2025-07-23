#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

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
                        size_t& hammock_length,
                        bool& elf,
                        bool& bin,
                        bool& only_dynamic,
                        bool& skip_non_user,
                        bool& show_disasm,
                        bool& enable_json) {
    trace_tools::CommandLineParser parser("stf_hammock");
    parser.addFlag('H', "hammock_length", "How many instructions can be skipped in the hammock. Defaults to 1.");
    parser.addFlag('e', "elf", "ELF file to search for always-skipped instructions");
    parser.addFlag('b', "bin", "Binary file to search for always-skipped instructions");
    parser.addFlag('o', "offset", "VA offset for raw binary files. Defaults to 0x80000000.");
    parser.addFlag('d', "only report dynamic branches (branches that are not always-taken or never-taken)");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('D', "show disassembly of hammocks");
    parser.addFlag('j', "enable JSON output");
    parser.addPositionalArgument("trace", "trace in STF format");

    parser.setMutuallyExclusive('e', 'b');
    parser.setMutuallyExclusive('e', 'o', "-o is only applicable to flat binary files.");
    parser.setDependentArgument('o', 'b', "-o option has no effect without -b");

    parser.parseArguments(argc, argv);

    hammock_length = 1;
    parser.getArgumentValue('H', hammock_length);

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
    enable_json = parser.hasArgument('j');

    parser.getPositionalArgument(0, trace);
}

struct HammockInfo {
    const uint32_t branch_opcode;
    const uint64_t inst_pc;
    const uint32_t inst_opcode;
    const std::string mnemonic;

    HammockInfo(const uint32_t branch_opcode, const uint64_t inst_pc, const uint32_t inst_opcode, const std::string& mnemonic) :
        branch_opcode(branch_opcode),
        inst_pc(inst_pc),
        inst_opcode(inst_opcode),
        mnemonic(mnemonic)
    {
    }

};

template<int COLUMN_WIDTH, int COLUMN_INDEX>
void formatDisasmLine(std::ostream& os, const stf::Disassembler& dis, const uint64_t pc, const uint32_t opcode) {
    static_assert(COLUMN_WIDTH >= 16, "Column width must be >= 16");
    stf::format_utils::formatSpaces(os, COLUMN_INDEX * COLUMN_WIDTH);
    stf::format_utils::formatHex(os, pc);
    stf::format_utils::formatSpaces(os, COLUMN_WIDTH - 16);
    dis.printDisassembly(os, pc, opcode);
    os << std::endl;
}

int main(int argc, char** argv) {
    std::string trace;
    std::string binary_file;
    size_t hammock_length;
    uint64_t binary_offset = 0;
    bool elf = false;
    bool bin = false;
    bool only_dynamic = false;
    bool skip_non_user = false;
    bool show_disasm = false;
    bool enable_json = false;

    try {
        processCommandLine(argc, argv, trace, binary_file, binary_offset, hammock_length, elf, bin, only_dynamic, skip_non_user, show_disasm, enable_json);
        if(hammock_length != 1) {
            throw trace_tools::CommandLineParser::EarlyExitException(1,
                                                                     "Only length-1 hammock branches are currently supported.");
        }
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
    std::set<uint64_t> hammock_branches;
    std::map<uint64_t, HammockInfo> hammock_info;

    // First find all instances of a not-taken conditional branch
    for(const auto& inst: reader) {
        // If it's a taken branch, we won't be able to tell what the next sequential instruction is.
        // So, we count it and check later if it was an actual hammock candidate
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
                        hammock_branches.insert(inst.pc());
                        hammock_info.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(inst.pc()),
                                             std::forward_as_tuple(inst.opcode(),
                                                                   next_pc,
                                                                   opcode,
                                                                   decoder.getMnemonic()));
                    }
                }
            }
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

        if(STF_EXPECT_FALSE(last_was_nt_branch)) {
            const auto next_pc = inst.pc() + inst.opcodeSize();
            if(last_branch_target == next_pc) {
                hammock_branches.insert(last_branch_pc);
                hammock_info.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(last_branch_pc),
                                     std::forward_as_tuple(last_branch_opcode,
                                                           inst.pc(),
                                                           inst.opcode(),
                                                           decoder.getMnemonic()));
            }
        }

        last_was_nt_branch = false;
    }

    static constexpr int COLUMN_WIDTH = 20;
    std::map<std::string, size_t> mnemonic_counts;
    std::ostringstream os;
    rapidjson::Document d(rapidjson::kObjectType);
    auto& d_alloc = d.GetAllocator();
    rapidjson::Value pc_results(rapidjson::kObjectType);

    for(const auto pc: hammock_branches) {
        const auto& counts = branch_counts[pc];
        const auto taken = counts.first;
        const auto not_taken = counts.second;
        if(only_dynamic && !(taken && not_taken)) {
            continue;
        }
        const auto total = taken + not_taken;
        const auto& hammock = hammock_info.at(pc);
        mnemonic_counts[hammock.mnemonic] += total;

        if(enable_json) {
            rapidjson::Value pc_result(rapidjson::kObjectType);
            pc_result.AddMember("total", (uint64_t)total, d_alloc);
            pc_result.AddMember("taken", (uint64_t)taken, d_alloc);
            pc_result.AddMember("not_taken", (uint64_t)not_taken, d_alloc);
            pc_result.AddMember("mnemonic", rapidjson::Value(hammock.mnemonic.c_str(), d_alloc).Move(), d_alloc);
            os << std::hex << pc;
            pc_results.AddMember(rapidjson::Value(os.str().c_str(), d_alloc).Move(), pc_result, d_alloc);
            os.str("");
        }
        else {
            stf::format_utils::formatHex(os, pc);
            stf::format_utils::formatSpaces(os, 4);
            stf::format_utils::formatDecLeft(os, total, COLUMN_WIDTH);
            stf::format_utils::formatDecLeft(os, taken, COLUMN_WIDTH);
            stf::format_utils::formatDecLeft(os, not_taken, COLUMN_WIDTH);
            stf::format_utils::formatLeft(os, hammock.mnemonic, COLUMN_WIDTH);
            os << std::endl;

            if(show_disasm) {
                formatDisasmLine<COLUMN_WIDTH, 4>(os, dis, pc, hammock.branch_opcode);
                formatDisasmLine<COLUMN_WIDTH, 4>(os, dis, hammock.inst_pc, hammock.inst_opcode);
            }
        }
    }

    if(enable_json) {
        d.AddMember("per_pc", pc_results, d_alloc);

        rapidjson::Value mnemonic_json(rapidjson::kObjectType);
        for(const auto& mnemonic_info: mnemonic_counts) {
            mnemonic_json.AddMember(rapidjson::Value(mnemonic_info.first.c_str(), d_alloc).Move(), (uint32_t)mnemonic_info.second, d_alloc);
        }
        d.AddMember("per_mnemonic", mnemonic_json, d_alloc);
        rapidjson::OStreamWrapper osw(std::cout);
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        d.Accept(writer);
        std::cout << std::endl;
    }
    else {
        stf::print_utils::printLeft("Mnemonic", COLUMN_WIDTH);
        stf::print_utils::printLeft("Total Instances", COLUMN_WIDTH);
        std::cout << std::endl;
        for(const auto& mnemonic_info: mnemonic_counts) {
            stf::print_utils::printLeft(mnemonic_info.first, COLUMN_WIDTH);
            stf::print_utils::printDecLeft(mnemonic_info.second, COLUMN_WIDTH);
            std::cout << std::endl;
        }
        std::cout << std::endl;

        stf::print_utils::printLeft("Start PC", COLUMN_WIDTH);
        stf::print_utils::printLeft("Total Instances", COLUMN_WIDTH);
        stf::print_utils::printLeft("Taken Count", COLUMN_WIDTH);
        stf::print_utils::printLeft("Not Taken Count", COLUMN_WIDTH);
        stf::print_utils::printLeft("Inst Mnemonic", COLUMN_WIDTH);
        if(show_disasm) {
            stf::print_utils::printLeft("Instruction PC", COLUMN_WIDTH);
            stf::print_utils::printLeft("Disassembly", COLUMN_WIDTH);
        }
        std::cout << std::endl << os.str() << std::endl;
    }

    return 0;
}
