#include <iostream>
#include <map>

#include "format_utils.hpp"
#include "stf_inst_reader.hpp"
#include "stf_reader.hpp"
#include "stf_record_types.hpp"

#include "command_line_parser.hpp"
#include "file_utils.hpp"
#include "tools_util.hpp"

/**
 * Parses command line options
 * \param argc argc from main
 * \param argv argv from main
 * \param trace_filename Trace to open
 * \param output_filename Destination file for address map
 * \param alignment Granularity used to determine address overlap
 * \param min_accesses Minimum number of accesses in report
 * \param user_mode_only Set to true if non-user mode skipping should be enabled
 * \param include_instruction_pc Set to true if instruction PCs should also be counted
 * \param only_instruction_pc Set to true if only instruction PCs should also be counted
 */
void parseCommandLine(int argc,
                      char** argv,
                      std::string& trace_filename,
                      std::string& output_filename,
                      uint64_t& alignment,
                      uint64_t& min_accesses,
                      bool& user_mode_only,
                      bool& include_instruction_pc,
                      bool& only_instruction_pc) {
    trace_tools::CommandLineParser parser("stf_address_map");

    parser.addFlag('o', "output", "output file (defaults to stdout)");
    parser.addFlag('a', "alignment", "align addresses to the specified number of bytes");
    parser.addFlag('m', "accesses", "restrict report to addresses with at least this many accesses");
    parser.addFlag('u', "only dump user-mode instructions");
    parser.addFlag('i', "count instruction PCs");
    parser.addFlag('I', "only count instruction PCs (implies -i)");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.appendHelpText("Example:");
    parser.appendHelpText("    List all addresses accessed at least 32 times, aligned to an 8-byte address");
    parser.appendHelpText("    stf_address_map -a 8 -m 32 trace.zstf");

    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);
    parser.getArgumentValue('a', alignment);
    parser.getArgumentValue('m', min_accesses);
    user_mode_only = parser.hasArgument('u');
    only_instruction_pc = parser.hasArgument('I');
    include_instruction_pc = only_instruction_pc || parser.hasArgument('i');
    parser.getPositionalArgument(0, trace_filename);
}

struct MemAccessCount {
    uint64_t reads = 0;
    uint64_t writes = 0;
};

using AddressMap = std::map<uint64_t, MemAccessCount>;

inline void countAddress(AddressMap& address_map, const stf::InstMemAccessRecord& mem_rec, const uint64_t address_mask) {
    auto& count = address_map[mem_rec.getAddress() & address_mask];

    if(mem_rec.getType() == stf::INST_MEM_ACCESS::READ) {
        ++count.reads;
    }
    else if(mem_rec.getType() == stf::INST_MEM_ACCESS::WRITE) {
        ++count.writes;
    }
}

inline void countPC(AddressMap& address_map, const uint64_t pc, const uint64_t address_mask) {
    ++address_map[pc & address_mask].reads;
}

int main(int argc, char** argv) {
    std::string trace_filename;
    std::string output_filename = "-";
    uint64_t alignment = 1;
    uint64_t min_accesses = 0;
    bool user_mode_only = false;
    bool include_instruction_pc = false;
    bool only_instruction_pc = false;

    try {
        parseCommandLine(argc,
                         argv,
                         trace_filename,
                         output_filename,
                         alignment,
                         min_accesses,
                         user_mode_only,
                         include_instruction_pc,
                         only_instruction_pc);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    OutputFileStream output_file(output_filename);
    AddressMap address_map;
    const uint64_t address_mask = std::numeric_limits<uint64_t>::max() << log2(alignment);

    if(user_mode_only) {
        stf::STFInstReader reader(trace_filename, user_mode_only);
        for(const auto& inst: reader) {
            if(!only_instruction_pc) {
                for(const auto& access: inst.getMemoryAccesses()) {
                    countAddress(address_map, access.getAccessRecord(), address_mask);
                }
            }
            if(include_instruction_pc) {
                countPC(address_map, inst.pc(), address_mask);
            }
        }
    }
    // Use STFReader if we don't care about instruction skipping since it's ~2x faster
    else {
        stf::STFReader reader(trace_filename);
        try {
            stf::STFRecord::UniqueHandle rec;
            while(reader >> rec) {
                if(STF_EXPECT_FALSE(!only_instruction_pc &&
                                    rec->getId() == stf::descriptors::internal::Descriptor::STF_INST_MEM_ACCESS)) {
                    countAddress(address_map, rec->as<stf::InstMemAccessRecord>(), address_mask);
                }
                else if(include_instruction_pc) {
                    const auto desc = rec->getId();
                    if(STF_EXPECT_FALSE(desc == stf::descriptors::internal::Descriptor::STF_INST_OPCODE16)) {
                        countPC(address_map, rec->as<stf::InstOpcode16Record>().getPC(), address_mask);
                    }
                    else if(STF_EXPECT_FALSE(desc == stf::descriptors::internal::Descriptor::STF_INST_OPCODE32)) {
                        countPC(address_map, rec->as<stf::InstOpcode32Record>().getPC(), address_mask);
                    }
                }
            }
        }
        catch(const stf::EOFException&) {
        }
    }

    static constexpr int COLUMN_WIDTH = 20;
    stf::format_utils::formatLeft(output_file, "Address", COLUMN_WIDTH);
    stf::format_utils::formatLeft(output_file, "Reads", COLUMN_WIDTH);
    stf::format_utils::formatLeft(output_file, "Writes", COLUMN_WIDTH);
    stf::format_utils::formatLeft(output_file, "Total", COLUMN_WIDTH);
    output_file << std::endl;
    for(const auto& p: address_map) {
        const auto reads = p.second.reads;
        const auto writes = p.second.writes;
        const auto total = reads + writes;
        if(total < min_accesses) {
            continue;
        }
        stf::format_utils::formatHex(output_file, p.first);
        stf::format_utils::formatSpaces(output_file, 4);
        stf::format_utils::formatDecLeft(output_file, reads, COLUMN_WIDTH);
        stf::format_utils::formatDecLeft(output_file, writes, COLUMN_WIDTH);
        stf::format_utils::formatDecLeft(output_file, total, COLUMN_WIDTH);
        output_file << std::endl;
    }

    return 0;
}
