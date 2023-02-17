#include <iostream>
#include <map>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include "command_line_parser.hpp"
#include "filesystem.hpp"
#include "print_utils.hpp"
#include "stf_reader.hpp"
#include "stf_record_types.hpp"
#include "tools_util.hpp"

static void parseCommandLine (int argc,
                              char **argv,
                              std::map<size_t, std::string>& traces,
                              bool& print_all,
                              bool& verbose,
                              bool& format_json) {
    trace_tools::CommandLineParser parser("stf_count_multi_process");
    parser.addFlag('a', "Print counts for instructions before the first user process");
    parser.addFlag('v', "Print which traces contain each process");
    parser.addFlag('j', "Output in JSON format");
    parser.addPositionalArgument("trace", "trace in STF format", true);
    parser.parseArguments(argc, argv);

    print_all = parser.hasArgument('a');
    verbose = parser.hasArgument('v');
    format_json = parser.hasArgument('j');
    const auto& trace_list = parser.getMultipleValuePositionalArgument(0);
    for(size_t i = 0; i < trace_list.size(); ++i) {
        traces.emplace(i, trace_list[i]);
    }
}

int main(int argc, char* argv[]) {
    std::map<size_t, std::string> traces;
    bool print_all = false;
    bool verbose = false;
    bool format_json = false;

    try {
        parseCommandLine(argc, argv, traces, print_all, verbose, format_json);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    std::map<uint64_t, std::map<size_t, uint64_t>> process_instruction_counts;
    uint64_t num_insts = 0;

    for(const auto& trace_pair: traces) {
        const auto trace_id = trace_pair.first;
        const auto& trace = trace_pair.second;

        stf::STFReader reader(trace);

        process_instruction_counts[0][trace_id] = 0;

        try {
            stf::STFRecord::UniqueHandle rec;
#ifdef COUNT_CONTEXT_SWITCHES
            uint64_t num_context_switches = 0;
#endif
            uint64_t cur_satp = 0;

            while(reader >> rec) {
                if(STF_EXPECT_FALSE(rec->getId() == stf::descriptors::internal::Descriptor::STF_INST_REG)) {
                    const auto& reg_rec = rec->as<stf::InstRegRecord>();
                    if(STF_EXPECT_FALSE((reg_rec.getOperandType() == stf::Registers::STF_REG_OPERAND_TYPE::REG_DEST) &&
                                        (reg_rec.getReg() == stf::Registers::STF_REG::STF_REG_CSR_SATP))) {
                        const uint64_t new_satp_value = reg_rec.getScalarData();
                        // Ignore the initial zeroing out of the satp register
                        process_instruction_counts[new_satp_value].try_emplace(trace_id, 0);
                        cur_satp = new_satp_value;
                    }
                }
#ifdef COUNT_CONTEXT_SWITCHES
                else if(STF_EXPECT_FALSE(rec->getId() == stf::descriptors::internal::Descriptor::STF_EVENT)) {
                    const auto& event_rec = rec->as<stf::EventRecord>();
                    if(event_rec.isModeChange()) {
                        const auto new_mode = static_cast<stf::EXECUTION_MODE>(event_rec.getData().front());
                        if(new_mode == stf::EXECUTION_MODE::USER_MODE) {
                            ++num_context_switches;
                        }
                    }
                }
#endif
                else if(STF_EXPECT_FALSE(rec->isInstructionRecord())) {
                    ++process_instruction_counts[cur_satp][trace_id];
                    ++num_insts;
                }
            }
        }
        catch(const stf::EOFException&) {
        }
    }

    std::ostringstream ss;

    if(format_json) {
        rapidjson::Document d(rapidjson::kObjectType);
        auto& d_alloc = d.GetAllocator();
        rapidjson::Value object(rapidjson::kObjectType);

        object.AddMember("num_traces", (uint32_t)traces.size(), d_alloc);
        object.AddMember("num_processes", (uint32_t)process_instruction_counts.size() - 1, d_alloc);
        object.AddMember("num_insts", (uint64_t)num_insts, d_alloc);

        rapidjson::Value processes_json(rapidjson::kObjectType);
        for(const auto& p: process_instruction_counts) {
            if(STF_EXPECT_FALSE(!print_all && p.first == 0)) {
                continue;
            }

            uint64_t process_num_insts = 0;
            rapidjson::Value process_json(rapidjson::kObjectType);
            rapidjson::Value traces_json(rapidjson::kArrayType);
            for(const auto& pp: p.second) {
                process_num_insts += pp.second;
                traces_json.PushBack(rapidjson::Value(fs::path(traces[pp.first]).filename().c_str(), d_alloc).Move(),
                                     d_alloc);
            }
            process_json.AddMember("num_insts", process_num_insts, d_alloc);
            process_json.AddMember("traces", traces_json, d_alloc);
            ss << std::hex << p.first;
            processes_json.AddMember(rapidjson::Value(ss.str().c_str(), d_alloc).Move(),
                                     process_json,
                                     d_alloc);
            ss.str("");
        }
        object.AddMember("processes", processes_json, d_alloc);
        rapidjson::OStreamWrapper osw(std::cout);
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        object.Accept(writer);
        std::cout << std::endl;
    }
    else {
        static constexpr int PROCESS_ID_COLUMN_WIDTH = 18;
        static constexpr int INSTRUCTION_COUNT_COLUMN_WIDTH = 17;
        static constexpr int TRACE_COUNT_COLUMN_WIDTH = 11;
        static constexpr int COLUMN_SEPARATOR_WIDTH = 4;
        std::cout << "Number of traces: " << traces.size() << std::endl
                  << "Number of processes: " << process_instruction_counts.size() - 1 << std::endl
                  << "Number of instructions: " << num_insts << std::endl;

        stf::print_utils::printLeft("Process", PROCESS_ID_COLUMN_WIDTH);
        stf::print_utils::printSpaces(COLUMN_SEPARATOR_WIDTH);
        stf::print_utils::printLeft("Instruction Count");
        stf::print_utils::printSpaces(COLUMN_SEPARATOR_WIDTH);
        if(verbose) {
            stf::print_utils::printLeft("Traces");
        }
        else {
            stf::print_utils::printLeft("Trace Count");
        }

        std::cout << std::endl;

        for(const auto& p: process_instruction_counts) {
            if(STF_EXPECT_FALSE(!print_all && p.first == 0)) {
                continue;
            }

            stf::print_utils::printHex(p.first, PROCESS_ID_COLUMN_WIDTH);
            stf::print_utils::printSpaces(COLUMN_SEPARATOR_WIDTH);
            uint64_t process_num_insts = 0;
            bool first = true;
            for(const auto& pp: p.second) {
                process_num_insts += pp.second;
                if(verbose) {
                    if(STF_EXPECT_FALSE(first)) {
                        first = false;
                    }
                    else {
                        stf::format_utils::formatSpaces(ss,
                                                        PROCESS_ID_COLUMN_WIDTH +
                                                        COLUMN_SEPARATOR_WIDTH +
                                                        INSTRUCTION_COUNT_COLUMN_WIDTH +
                                                        COLUMN_SEPARATOR_WIDTH);
                    }
                    ss << fs::path(traces[pp.first]).filename().c_str() << std::endl;
                }
            }
            stf::print_utils::printDec(process_num_insts, INSTRUCTION_COUNT_COLUMN_WIDTH, ' ');
            stf::print_utils::printSpaces(COLUMN_SEPARATOR_WIDTH);
            if(verbose) {
                std::cout << ss.str();
                ss.str("");
            }
            else {
                stf::print_utils::printDec(p.second.size(), TRACE_COUNT_COLUMN_WIDTH, ' ');
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
