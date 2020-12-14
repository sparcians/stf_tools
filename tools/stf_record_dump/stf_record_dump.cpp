#include <iostream>

#include "print_utils.hpp"
#include "stf_reader.hpp"
#include "stf_record_types.hpp"

#include "command_line_parser.hpp"
#include "stf_record_dump.hpp"
#include "tools_util.hpp"

static STFRecordDumpConfig parseCommandLine(int argc, char **argv) {
    // Parse options
    STFRecordDumpConfig config;
    trace_tools::CommandLineParser parser("stf_record_dump");
    parser.addFlag('u', "only dump user-mode instructions");
    parser.addFlag('p', "show physical addresses");
    parser.addFlag('P', "show the PTE entries");
    parser.addFlag('A', "use aliases for disassembly");
    parser.addFlag('m', "Enables cross checking trace instruction opcode against opcode from symbol table file <*_symTab.yaml>. Applicable only when '-y' flag is enabled");
    parser.addFlag('s', "N", "start dumping at N-th instruction");
    parser.addFlag('e', "M", "end dumping at M-th instruction");
    parser.addFlag('S', "N", "start dumping at N-th record");
    parser.addFlag('E', "M", "end dumping at M-th record");
    parser.addFlag('y', "*_symTab.yaml", "YAML symbol table file to show annotation");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    config.use_aliases = parser.hasArgument('A');
    parser.getArgumentValue('s', config.start_inst);
    parser.getArgumentValue('e', config.end_inst);
    parser.getArgumentValue('S', config.start_record);
    parser.getArgumentValue('E', config.end_record);
    config.show_annotation = parser.getArgumentValue('y', config.symbol_filename);

    parser.getPositionalArgument(0, config.trace_filename);

    stf_assert(!config.end_inst || (config.end_inst > config.start_inst),
               "End inst (" << config.end_inst << ") must be greater than start inst (" << config.start_inst << ')');

    stf_assert(!config.end_record || (config.end_record > config.start_record),
               "End record (" << config.end_record << ") must be greater than start record (" << config.start_record << ')');

    stf_assert(!config.start_record || !config.start_inst, "Cannot specify both -s and -S options at the same time.");
    stf_assert(!config.end_record || !config.end_inst, "Cannot specify both -e and -E options at the same time.");

    return config;
}

int main (int argc, char **argv)
{
    try {
        // Get arguments
        const STFRecordDumpConfig config = parseCommandLine(argc, argv);

        // Open stf trace reader
        stf::STFReader stf_reader(config.trace_filename);
        stf_reader.checkVersion();

        if(config.start_inst || config.end_inst) {
            std::cout << "Start Inst:" << config.start_inst;

            if(config.end_inst) {
                std::cout << "  End Inst:" << config.end_inst << std::endl;
            }
            else {
                std::cout << std::endl;
            }
        }

        if(config.start_record || config.end_record) {
            std::cout << "Start Record:" << config.start_record;

            if(config.end_record) {
                std::cout << "  End Record:" << config.end_record << std::endl;
            }
            else {
                std::cout << std::endl;
            }
        }

        stf_reader.dumpHeader(std::cout);

        try {
            stf::STFRecord::UniqueHandle rec;
            if(config.start_inst > 1) {
                stf_reader.seek(config.start_inst - 1);
            }
            else if(config.start_record > 1) {
                while(stf_reader >> rec) {
                    if(stf_reader.numRecordsRead() == config.start_record) {
                        break;
                    }
                }
            }

            while(stf_reader >> rec) {
                rec->format(std::cout);
                std::cout << std::endl;
                if(STF_EXPECT_FALSE((config.end_inst && (stf_reader.numInstsRead() == config.end_inst)) ||
                                    (config.end_record && (stf_reader.numRecordsRead() == config.end_record)))) {
                    break;
                }
            }
        }
        catch(const stf::EOFException&) {
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
