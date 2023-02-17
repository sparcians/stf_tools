
// <STF_dump> -*- C++ -*-

/**
 * \brief  This tool prints out the content of a STF trace file
 *
 */

#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include "command_line_parser.hpp"
#include "disassembler.hpp"
#include "print_utils.hpp"
#include "stf_dump.hpp"
#include "stf_inst_reader.hpp"
#include "stf_page_table.hpp"
#include "tools_util.hpp"
//#include "STFSymTab.hpp"

static STFDumpConfig parseCommandLine (int argc, char **argv) {
    // Parse options
    STFDumpConfig config;
    trace_tools::CommandLineParser parser("stf_dump");

    parser.addFlag('c', "concise mode - only dumps PC information and disassembly");
    parser.addFlag('u', "only dump user-mode instructions");
    parser.addFlag('p', "show physical addresses");
    parser.addFlag('P', "show the PTE entries");
    parser.addFlag('A', "use aliases for disassembly (only used by binutils disassembler)");
    parser.addFlag('m', "Enables cross checking trace instruction opcode against opcode from symbol table file <*_symTab.yaml>. Applicable only when '-y' flag is enabled");
    parser.addFlag('s', "N", "start dumping at N-th instruction");
    parser.addFlag('e', "M", "end dumping at M-th instruction");
    parser.addFlag('y', "*_symTab.yaml", "YAML symbol table file to show annotation");
    parser.addFlag('H', "omit the header information");
    parser.addPositionalArgument("trace", "trace in STF format");

    parser.parseArguments(argc, argv);

    config.concise_mode = parser.hasArgument('c');
    config.user_mode_only = parser.hasArgument('u');
    stf::format_utils::setShowPhys(parser.hasArgument('p'));
    config.show_pte = parser.hasArgument('P');
    config.use_aliases = parser.hasArgument('A');
    config.match_symbol_opcode = parser.hasArgument('m');

    parser.getArgumentValue('s', config.start_inst);
    parser.getArgumentValue('e', config.end_inst);
    parser.getArgumentValue('y', config.symbol_filename);
    config.show_annotation = !config.symbol_filename.empty();
    config.omit_header = parser.hasArgument('H');
    parser.getPositionalArgument(0, config.trace_filename);

    stf_assert(!config.end_inst || (config.end_inst >= config.start_inst),
               "End inst (" << config.end_inst << ") must be greater than or equal to start inst (" << config.start_inst << ')');

    return config;
}

/**
 * Prints an opcode along with its disassembly
 * \param opcode instruction opcode
 * \param pc instruction PC
 * \param os The ostream to write the assembly to
 */
static inline void printOpcodeWithDisassembly(const stf::Disassembler& dis,
                                              const uint32_t opcode,
                                              const uint64_t pc) {
    static constexpr int OPCODE_PADDING = stf::format_utils::OPCODE_FIELD_WIDTH - stf::format_utils::OPCODE_WIDTH - 1;

    dis.printOpcode(std::cout, opcode);

    stf::print_utils::printSpaces(OPCODE_PADDING); // pad out the rest of the opcode field with spaces

    dis.printDisassembly(std::cout, pc, opcode);
    // if (show_annotation)
    // {
    //     // Retrieve symbol information from symbol table hash map
    //     symInfo = annoSt->getSymbol(dism_pc);
    //     if (match_symbol_opcode)
    //     {
    //         if(symInfo.opcode != inst.opcode())
    //             std::cout << " | " << " [ " << symInfo.libName << ", " << symInfo.symName << ", OPCODE_MISMATCH: " << std::hex  << symInfo.opcode << " ] ";
    //         else
    //             std::cout << " | " << " [ " << symInfo.libName << ", " << symInfo.symName << ", OPCODE_CROSSCHECKED"  << " ] ";
    //     }
    //     else
    //         std::cout << " | " << " [ " << symInfo.libName << ", " << symInfo.symName << " ] ";
    // }
    std::cout << std::endl;
}

int main (int argc, char **argv)
{
    // Get arguments
    try {
        const STFDumpConfig config = parseCommandLine (argc, argv);

        // Open stf trace reader
        stf::STFInstReader stf_reader(config.trace_filename, config.user_mode_only);
        stf_reader.checkVersion();

        // Create disassembler
        stf::Disassembler dis(stf_reader.getISA(), stf_reader.getInitialIEM(), config.use_aliases);

        if(!config.omit_header) {
            // Print Version info
            stf::print_utils::printLabel("VERSION");
            std::cout << stf_reader.major() << '.' << stf_reader.minor() << std::endl;

            // Print trace info
            for(const auto& i: stf_reader.getTraceInfo()) {
                std::cout << *i;
            }

            // Print Instruction set info
            stf::print_utils::printLabel("INST_IEM");
            std::cout << stf_reader.getISA() << std::endl;

            if(config.start_inst || config.end_inst) {
                std::cout << "Start Inst:" << config.start_inst;

                if(config.end_inst) {
                    std::cout << "  End Inst:" << config.end_inst << std::endl;
                }
                else {
                    std::cout << std::endl;
                }
            }
        }

        if (config.show_pte) {
            for (auto it = stf_reader.pteBegin(); it != stf_reader.pteEnd(); ++it) {
                const auto& pte = *it;
                std::cout << pte;
            }
        }

        uint32_t tid_prev = std::numeric_limits<uint32_t>::max();
        uint32_t pid_prev = std::numeric_limits<uint32_t>::max();
        uint32_t asid_prev = std::numeric_limits<uint32_t>::max();

        const auto start_inst = config.start_inst ? config.start_inst - 1 : 0;

        for (auto it = stf_reader.begin(start_inst); it != stf_reader.end(); ++it) {
            const auto& inst = *it;

            if (STF_EXPECT_FALSE(!inst.valid())) {
                std::cerr << "ERROR: " << inst.index() << " invalid instruction " << std::hex << inst.opcode() << " PC " << inst.pc() << std::endl;
            }

            const uint32_t tid = inst.tid();
            const uint32_t pid = inst.tgid();
            const uint32_t asid = inst.asid();
            if (STF_EXPECT_FALSE(!config.concise_mode && (tid != tid_prev || pid != pid_prev || asid != asid_prev))) {
                stf::print_utils::printLabel("PID");
                stf::print_utils::printTID(pid);
                std::cout << ':';
                stf::print_utils::printTID(tid);
                std::cout << ':';
                stf::print_utils::printTID(asid);
                std::cout << std::endl;
            }
            tid_prev = tid;
            pid_prev = pid;
            asid_prev = asid;

            // Opcode width string (INST32/INST16) and index should each take up half of the label column
            stf::print_utils::printLeft(inst.getOpcodeWidthStr(), stf::format_utils::LABEL_WIDTH / 2);

            stf::print_utils::printDecLeft(inst.index(), stf::format_utils::LABEL_WIDTH / 2);

            stf::print_utils::printVA(inst.pc());

            if (stf::format_utils::showPhys()) {
                // Make sure we zero-fill as needed, so that the address remains "virt:phys" and not "virt:  phys"
                std::cout << ':';
                stf::print_utils::printPA(inst.getPA(inst.pc()));
            }
            stf::print_utils::printSpaces(1);

            if (STF_EXPECT_FALSE(inst.isTakenBranch())) {
                std::cout << "PC ";
                stf::print_utils::printVA(inst.branchTarget());
                stf::print_utils::printSpaces(1);
            }
            else if(STF_EXPECT_FALSE(config.concise_mode && (inst.isFault() || inst.isInterrupt()))) {
                const std::string_view fault_msg = inst.isFault() ? "FAULT" : "INTERRUPT";
                stf::print_utils::printLeft(fault_msg, stf::format_utils::VA_WIDTH + 4);
                if (stf::format_utils::showPhys()) {
                    stf::print_utils::printSpaces(stf::format_utils::PA_WIDTH + 1);
                }
            }
            else {
                stf::print_utils::printSpaces(stf::format_utils::VA_WIDTH + 4);
                if (stf::format_utils::showPhys()) {
                    stf::print_utils::printSpaces(stf::format_utils::PA_WIDTH + 1);
                }
            }

            stf::print_utils::printSpaces(9); // Additional padding so that opcode lines up with operand values
            printOpcodeWithDisassembly(dis, inst.opcode(), inst.pc());

            if(!config.concise_mode) {
                for(const auto& m: inst.getMemoryAccesses()) {
                    std::cout << m << std::endl;
                }

                if (config.show_pte) {
                    for(const auto& pte: inst.getEmbeddedPTEs()) {
                        std::cout << pte->as<stf::PageTableWalkRecord>();
                    }
                }

                for(const auto& reg: inst.getRegisterStates()) {
                    std::cout << reg << std::endl;
                }

                for(const auto& reg: inst.getOperands()) {
                    std::cout << reg << std::endl;
                }

                for(const auto& evt: inst.getEvents()) {
                    std::cout << evt << std::endl;
                }

                for(const auto& cmt: inst.getComments()) {
                    std::cout << cmt->as<stf::CommentRecord>() << std::endl;
                }

                for(const auto& uop: inst.getMicroOps()) {
                    const auto& microop = uop->as<stf::InstMicroOpRecord>();
                    stf::print_utils::printOperandLabel(microop.getSize() == 2 ? "UOp16 " : "UOp32 ");
                    stf::print_utils::printSpaces(stf::format_utils::REGISTER_NAME_WIDTH + stf::format_utils::DATA_WIDTH);
                    printOpcodeWithDisassembly(dis, microop.getMicroOp(), inst.pc());
                }

                for(const auto& reg: inst.getReadyRegs()) {
                    stf::print_utils::printOperandLabel("ReadyReg ");
                    std::cout << std::dec << reg->as<stf::InstReadyRegRecord>().getReg() << std::endl;
                }
            }

            if (STF_EXPECT_FALSE(config.end_inst && (inst.index() >= config.end_inst))) {
                break;
            }
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }
    //delete annoSt;
    return 0;
}
