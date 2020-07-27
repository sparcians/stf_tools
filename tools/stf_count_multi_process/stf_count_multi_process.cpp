#include <iostream>
#include <map>

#include "command_line_parser.hpp"
#include "print_utils.hpp"
#include "stf_reader.hpp"
#include "stf_record_types.hpp"
#include "tools_util.hpp"

static std::string parseCommandLine (int argc, char **argv) {
    trace_tools::CommandLineParser parser("stf_count_multi_process");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    return parser.getPositionalArgument<std::string>(0);
}

int main(int argc, char* argv[]) {
    try {
        stf::STFReader reader(parseCommandLine(argc, argv));

        std::map<uint64_t, uint64_t> process_instruction_counts;
        process_instruction_counts[0] = 0;
        uint64_t num_insts = 0;

        try {
            stf::STFRecord::UniqueHandle rec;
            uint64_t num_context_switches = 0;
            uint64_t cur_satp = 0;

            while(reader >> rec) {
                if(STF_EXPECT_FALSE(rec->getDescriptor() == stf::descriptors::internal::Descriptor::STF_INST_REG)) {
                    const auto& reg_rec = rec->as<stf::InstRegRecord>();
                    if(STF_EXPECT_FALSE((reg_rec.getOperandType() == stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST) &&
                                        (reg_rec.getReg() == stf::Registers::STF_REG::STF_REG_CSR_SATP))) {
                        const uint64_t new_satp_value = reg_rec.getData();
                        // Ignore the initial zeroing out of the satp register
                        process_instruction_counts.emplace(new_satp_value, 0);
                        cur_satp = new_satp_value;
                    }
                }
                else if(STF_EXPECT_FALSE(rec->getDescriptor() == stf::descriptors::internal::Descriptor::STF_EVENT)) {
                    const auto& event_rec = rec->as<stf::EventRecord>();
                    if(event_rec.isModeChange()) {
                        const auto new_mode = static_cast<stf::EXECUTION_MODE>(event_rec.getData().front());
                        if(new_mode == stf::EXECUTION_MODE::USER_MODE) {
                            ++num_context_switches;
                        }
                    }
                }
                else if(STF_EXPECT_FALSE(rec->isInstructionRecord())) {
                    ++process_instruction_counts[cur_satp];
                    ++num_insts;
                }
            }
        }
        catch(const stf::EOFException&) {
        }

        static constexpr int PROCESS_ID_COLUMN_WIDTH = 18;
        static constexpr int COLUMN_SEPARATOR_WIDTH = 4;
        std::cout << "Number of processes: " << process_instruction_counts.size() - 1 << std::endl;
        std::cout << "Number of instructions: " << num_insts << std::endl;

        stf::print_utils::printLeft("Process", PROCESS_ID_COLUMN_WIDTH);
        stf::print_utils::printSpaces(COLUMN_SEPARATOR_WIDTH);
        stf::print_utils::printLeft("Instruction Count");
        std::cout << std::endl;

        for(const auto& p: process_instruction_counts) {
            stf::print_utils::printHex(p.first, PROCESS_ID_COLUMN_WIDTH);
            stf::print_utils::printSpaces(COLUMN_SEPARATOR_WIDTH);
            stf::print_utils::printDec(p.second);
            std::cout << std::endl;
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
