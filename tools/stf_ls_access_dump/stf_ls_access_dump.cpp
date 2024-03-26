#include <iostream>
#include <string>
#include <cstdint>

#include "print_utils.hpp"
#include "stf_inst_reader.hpp"
#include "command_line_parser.hpp"
#include "dependency_tracker.hpp"
#include "tools_util.hpp"

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        bool& skip_non_user,
                        bool& csv) {
    trace_tools::CommandLineParser parser("stf_ls_access_dump");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addFlag('c', "output in CSV format");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    skip_non_user = parser.hasArgument('u');
    csv = parser.hasArgument('c');

    parser.getPositionalArgument(0, trace);
}

int main(int argc, char** argv) {
    std::string trace;
    bool skip_non_user = false;
    bool csv = false;

    try {
        processCommandLine(argc, argv, trace, skip_non_user, csv);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    stf::STFInstReader reader(trace, skip_non_user);

    const int index_width = csv ? 0 : stf::format_utils::LABEL_WIDTH;
    const int type_width = csv ? 0 : stf::format_utils::LABEL_WIDTH / 2;
    const int addr_width = csv ? 0 : stf::format_utils::VA_WIDTH;
    const char* const col_sep = csv ? "," : "";
    const char* const addr_sep = csv ? "," : "    ";

    stf::print_utils::printLeft("Index", index_width);
    stf::print_utils::printLeft(col_sep);
    stf::print_utils::printLeft("Type", type_width);
    stf::print_utils::printLeft(col_sep);
    stf::print_utils::printLeft("Address", addr_width);
    stf::print_utils::printLeft(addr_sep);
    stf::print_utils::printLeft("Data", addr_width);
    stf::print_utils::printLeft(addr_sep);
    stf::print_utils::printLeft("Size");

    std::cout << std::endl;

    for(const auto& inst: reader) {
        const bool load = inst.isLoad();
        const bool store = inst.isStore();

        if(STF_EXPECT_TRUE(!(load || store))) {
            continue;
        }

        stf::print_utils::printDecLeft(inst.index(), index_width);
        stf::print_utils::printLeft(col_sep);

        for(const auto& m: inst.getMemoryAccesses()) {
            switch(m.getType()) {
                case stf::INST_MEM_ACCESS::READ:
                    stf::print_utils::printLeft("READ", type_width);
                    break;
                case stf::INST_MEM_ACCESS::WRITE:
                    stf::print_utils::printLeft("WRITE", type_width);
                    break;
                default:
                    stf_throw("Unexpected INST_MEM_ACCESS type: " << m.getType());
            }

            stf::print_utils::printLeft(col_sep);

            stf::print_utils::printVA(m.getAddress());
            stf::print_utils::printLeft(addr_sep);
            stf::print_utils::printVA(m.getData());
            stf::print_utils::printLeft(addr_sep);
            stf::print_utils::printDecLeft(m.getSize());
            std::cout << std::endl;
        }
    }

    return 0;
}
