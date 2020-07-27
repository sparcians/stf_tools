
// <stf_verify_pc> -*- C++ -*-

/**
 * \brief  This tool verifies that all INST_PC records other than
 * the first one appears due to previous PC_TARGET = 0xFFFFxxxx
 *
 */

#include <array>
#include <iostream>
#include <string>
#include <iomanip>
#include <string_view>

#include "command_line_parser.hpp"
#include "print_utils.hpp"
#include "stf.hpp"
#include "stf_exception.hpp"
#include "stf_inst_reader.hpp"
#include "stf_verify_pc.hpp"
#include "tools_util.hpp"

static void parseCommandLine(int argc, char **argv, std::string& trace_filename) {
    trace_tools::CommandLineParser parser("stf_verify_pc");
    parser.addPositionalArgument("trace", "trace in STF format");

    parser.parseArguments(argc, argv);
    parser.getPositionalArgument(0, trace_filename);
}

void printError(const BranchTarget& bt, const stf::STFInst& inst) {
    static constexpr int COUNT_SIZE = 10;
    std::cout << "ERROR: Previous Branch Count ";
    stf::print_utils::printDec(bt.inst_count, COUNT_SIZE);
    std::cout << " Target ";
    stf::print_utils::printVA(bt.branch_target);
    std::cout << ", Current Count ";
    stf::print_utils::printDec(inst.index(), COUNT_SIZE);
    std::cout << " PC ";
    stf::print_utils::printVA(inst.pc());
    std::cout << std::endl;
}

int main (int argc, char **argv)
{
    std::string trace_filename;
    try {
        parseCommandLine(argc, argv, trace_filename);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    // Open stf trace reader
    stf::STFInstReader stf_reader(trace_filename);
    /* FIXME Because we have not kept up with STF versioning, this is currently broken and must be loosened.
    if (!stf_reader.checkVersion()) {
        exit(1);
    }
    */

    uint64_t PC_changes = 0;
    std::array<uint64_t, 2> PC_errors = { 0, 0 };
    BranchTarget bt = { 0, 0 };

    for (const auto& inst: stf_reader) {
        if (!inst.valid()) {
            std::cerr << "ERROR: "
                      << inst.index()
                      << " invalid instruction ";
            stf::format_utils::formatHex(std::cerr, inst.opcode());
            std:: cerr << " PC ";
            stf::format_utils::formatVA(std::cerr, inst.pc());
            std::cerr << std::endl;
        }

        if (inst.isCoF() && (inst.index() > 1)) {
            // contains an INST_PC record and not the first instruction
            ++PC_changes;
            if (inst.index() > (bt.inst_count + 1)) {
                // the previous instruction is not a branch
                ++PC_errors[0];
                printError(bt, inst);
            }
            else {
                stf_assert(inst.index() == (bt.inst_count + 1), "Inst index should at least be equal to the branch target instruction count");
                // the previous branch target is below 0xFFFF region
                if (bt.branch_target < 0xFFFF0000) {
                    printError(bt, inst);
                    ++PC_errors[1];
                }
            }
        }

        if (inst.isTakenBranch()) {
            bt.inst_count = inst.index();
            bt.branch_target = inst.branchTarget();
        }
    }

    std::cout << "STATS PC changes                       : " << PC_changes << std::endl
              << "STATS Previous inst not branch errors  : " << PC_errors[0] << std::endl
              << "STATS Previous branch not 0xFFFF errors: " << PC_errors[1] << std::endl;

    return 0;
}
