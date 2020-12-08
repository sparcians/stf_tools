#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "stf_decoder.hpp"

#include "print_utils.hpp"
#include "stf_inst_reader.hpp"
#include "command_line_parser.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        bool& verbose,
                        bool& only_taken,
                        bool& only_dynamic,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_branch_classify");
    parser.addFlag('v', "verbose mode (prints indirect branch targets)");
    parser.addFlag('t', "only report taken branches (branches that are taken at least once)");
    parser.addFlag('d', "only report dynamic branches (branches that are not always-taken or never-taken)");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    verbose = parser.hasArgument('v');
    only_taken = parser.hasArgument('t');
    only_dynamic = parser.hasArgument('d');
    skip_non_user = parser.hasArgument('u');

    parser.getPositionalArgument(0, trace);
}

enum class Direction : uint8_t {
    INVALID,
    FORWARD,
    BACKWARD,
    MULTIPLE
};

struct BranchInfo {
    uint64_t taken = 0;
    uint64_t not_taken = 0;
    std::map<uint64_t, uint64_t> targets;
    bool indirect = false;
    Direction direction = Direction::INVALID;
};

int main(int argc, char** argv) {
    std::string trace;
    bool verbose = false;
    bool only_taken = false;
    bool only_dynamic = false;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, verbose, only_taken, only_dynamic, skip_non_user);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    stf::STFInstReader reader(trace, skip_non_user);

    stf::STFDecoder decoder;
    std::map<uint64_t, BranchInfo> branch_counts;

    for(const auto& inst: reader) {
        // We can avoid decoding the instruction if we know for a fact it isn't a branch or a move
        if(STF_EXPECT_FALSE(inst.isLoad() || inst.isStore() || inst.isSyscall())) {
            continue;
        }

        decoder.decode(inst.opcode());

        if(STF_EXPECT_TRUE(!decoder.isBranch())) {
            continue;
        }

        const auto pc = inst.pc();
        uint64_t target = 0;
        auto& branch_info = branch_counts[pc];

        if(inst.isTakenBranch()) {
            target = inst.branchTarget();
            ++branch_info.taken;
        }
        // Compute the real target
        else {
            stf_assert(!decoder.isInstType(mavis::InstMetaData::InstructionTypes::JALR) &&
                       !decoder.isInstType(mavis::InstMetaData::InstructionTypes::JAL),
                       "JAL and JALR branches should be unconditional");
            target = inst.pc() + static_cast<uint64_t>(decoder.getSignedImmediate());
            ++branch_info.not_taken;
        }

        branch_info.targets[target] += inst.isTakenBranch();
        branch_info.indirect = decoder.isInstType(mavis::InstMetaData::InstructionTypes::JALR);

        const Direction new_dir = target <= pc ? Direction::BACKWARD : Direction::FORWARD;
        if(branch_info.direction == Direction::INVALID) {
            branch_info.direction = new_dir;
        }
        else if(branch_info.direction != new_dir) {
            stf_assert(branch_info.indirect, "Only indirect branches can have multiple directions");
            branch_info.direction = Direction::MULTIPLE;
        }
    }

    static constexpr int COLUMN_WIDTH = 20;
    stf::print_utils::printLeft("Instruction PC", COLUMN_WIDTH);
    stf::print_utils::printLeft("Total Instances", COLUMN_WIDTH);
    stf::print_utils::printLeft("Behavior", COLUMN_WIDTH);
    stf::print_utils::printLeft("Direction", COLUMN_WIDTH);
    stf::print_utils::printLeft("Indirect", COLUMN_WIDTH);
    stf::print_utils::printLeft("# Targets", COLUMN_WIDTH);
    stf::print_utils::printLeft("Taken Count", COLUMN_WIDTH);
    stf::print_utils::printLeft("Not Taken Count", COLUMN_WIDTH);
    if(verbose) {
        stf::print_utils::printLeft("Target", COLUMN_WIDTH);
        stf::print_utils::printLeft("Traversals", COLUMN_WIDTH);
    }
    std::cout << std::endl;
    for(const auto& branch: branch_counts) {
        const auto& branch_info = branch.second;
        const auto taken = branch_info.taken;
        const auto not_taken = branch_info.not_taken;

        if(only_dynamic && !(taken && not_taken)) {
            continue;
        }

        if(only_taken && !taken) {
            continue;
        }

        const auto pc = branch.first;
        const auto total = taken + not_taken;
        stf_assert(total, "Invalid branch behavior for pc " << std::hex << pc);

        stf::print_utils::printHex(pc);
        stf::print_utils::printSpaces(4);
        stf::print_utils::printDecLeft(total, COLUMN_WIDTH);

        if(taken && !not_taken) {
            stf::print_utils::printLeft("AT", COLUMN_WIDTH);
        }
        else if(!taken && not_taken) {
            stf::print_utils::printLeft("NT", COLUMN_WIDTH);
        }
        else if(taken && not_taken) {
            stf::print_utils::printLeft("DYN", COLUMN_WIDTH);
        }

        switch(branch_info.direction) {
            case Direction::FORWARD:
                stf::print_utils::printLeft("FORWARD", COLUMN_WIDTH);
                break;
            case Direction::BACKWARD:
                stf::print_utils::printLeft("BACKWARD", COLUMN_WIDTH);
                break;
            case Direction::MULTIPLE:
                stf::print_utils::printLeft("MULTIPLE", COLUMN_WIDTH);
                break;
            case Direction::INVALID:
                stf_throw("Invalid branch direction for pc " << std::hex << pc);
        };

        if(branch_info.indirect) {
            stf::print_utils::printLeft('Y', COLUMN_WIDTH);
        }
        else {
            stf::print_utils::printLeft('N', COLUMN_WIDTH);
        }

        stf::print_utils::printDecLeft(branch_info.targets.size(), COLUMN_WIDTH);

        stf::print_utils::printDecLeft(taken, COLUMN_WIDTH);
        stf::print_utils::printDecLeft(not_taken, COLUMN_WIDTH);

        if(verbose) {
            bool first_line = true;
            for(const auto& target_pair: branch_info.targets) {
                if(STF_EXPECT_FALSE(first_line)) {
                    first_line = false;
                }
                else {
                    stf::print_utils::printSpaces(8*COLUMN_WIDTH);
                }
                stf::print_utils::printHex(target_pair.first);
                stf::print_utils::printSpaces(4);
                stf::print_utils::printDecLeft(target_pair.second);
                std::cout << std::endl;
            }
        }

        std::cout << std::endl;
    }

    return 0;
}
