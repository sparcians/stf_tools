#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <cstdlib>
#include <iterator>
#include <regex>

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
 */
void parseCommandLine(int argc,
                      char** argv,
                      std::string& trace_filename,
                      std::string& output_filename,
                      uint64_t& alignment,
                      uint64_t& min_accesses,
                      bool& user_mode_only,
                      bool& include_instruction_pc,
                      bool& only_instruction_pc
                      , bool& exclude_reads, bool& exclude_writes
                      , uint64_t& max_distance_access, uint64_t& max_distance_stream
                      ) {
    trace_tools::CommandLineParser parser("stf_address_sequence");

    parser.addFlag('o', "output", "output file (defaults to stdout), if named batch, auto name based on trace, w/o top row in summary");
    parser.addFlag('a', "alignment", "align addresses to the specified number of bytes");
//  parser.addFlag('m', "accesses", "restrict report to addresses with at least this many accesses");
    parser.addFlag('u', "only dump user-mode instructions");
    parser.addFlag('i', "count instruction PCs");
    parser.addFlag('I', "only count instruction PCs (implies -i)");
    parser.addFlag('R', "exclude reads");
    parser.addFlag('W', "exclude writes");
    parser.addFlag('C', "max_distance_access", "restrict report to repeated accesses with at MOST this distance. Each memory transaction(RD or WR) counts one access");
    parser.addFlag('A', "max_distance_stream", "restrict report to repeated streams with at MOST this distance. Each gather counts one. Access pattern ABA counts 3, AABB counts 2");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.appendHelpText("Terminology:");
    parser.appendHelpText("    Repeated access  - The access pattern AAAAA doesn't count. AABAAAAA counts 5 repeated accesses to A. ABABABAB counts 3 repeated accesses to A, and 3 to B");
    parser.appendHelpText("    Repeated stream  - The access pattern AAAAA doesn't count. AABAAAAA counts 1 repeated stream to A. ABABABAB counts 3 repeated streams to A, and 3 to B, even if each stream consists of only one access");
    parser.appendHelpText("\nBackground:");
    parser.appendHelpText("    This program is created to examine temporal locality. So the distance constraints/filters are defined as max distance. If the 2 memory transactions are far apart, separated by other memory transactions, the 2nd one won't be considered as repeated access/stream, even though it accesses the same aligned address");
    parser.appendHelpText("    The access pattern AABBAA has 6 accesses, 3 streams. The distance is calculated based on ID shown below. From the second first A to the second last A, the access distance is 3, the stream distance is 2");
    parser.appendHelpText("                       --");
    parser.appendHelpText("                       strm0, access0, access1");
    parser.appendHelpText("                         --");
    parser.appendHelpText("                         strm1, access2, access3");
    parser.appendHelpText("                           --");
    parser.appendHelpText("                           strm2, access4, access5");
    parser.appendHelpText("\nExample:");
    parser.appendHelpText("    List all addresses, aligned to an 8-byte address");
    parser.appendHelpText("        stf_address_sequence -a 8 trace.zstf");
    parser.appendHelpText("\nNote:");
    parser.appendHelpText("    !!!warm-up isn't supported!!!");

    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);
    parser.getArgumentValue('a', alignment);
//  parser.getArgumentValue('m', min_accesses);
    user_mode_only = parser.hasArgument('u');
    only_instruction_pc = parser.hasArgument('I');
    include_instruction_pc = only_instruction_pc || parser.hasArgument('i');
    parser.getPositionalArgument(0, trace_filename);
    exclude_reads = parser.hasArgument('R');
    exclude_writes = parser.hasArgument('W');
    parser.getArgumentValue('C', max_distance_access);
    parser.getArgumentValue('A', max_distance_stream);
}

struct MemAccessCount {
    uint64_t reads = 0;
    uint64_t writes = 0;
};

using AddressMap = std::map<uint64_t, MemAccessCount>;

inline int countAddress(AddressMap& address_map, const stf::InstMemAccessRecord& mem_rec, const uint64_t address_mask, const bool exclude_reads, const bool exclude_writes) {
    if((exclude_reads && mem_rec.getType() == stf::INST_MEM_ACCESS::READ) ||
       (exclude_writes && mem_rec.getType() == stf::INST_MEM_ACCESS::WRITE)){
        return 1; // skip, do nothing
    }

    if (!address_map.empty() && address_map.find(mem_rec.getAddress() & address_mask) == address_map.end()) {
        return -1; // need a new entry
    }
    stf_assert(address_map.size() < 2, "address_map is too big")

    auto& count = address_map[mem_rec.getAddress() & address_mask];

    if(mem_rec.getType() == stf::INST_MEM_ACCESS::READ) {
        ++count.reads;
    }
    else if(mem_rec.getType() == stf::INST_MEM_ACCESS::WRITE) {
        ++count.writes;
    }
    return 0;
}

inline void countPC(AddressMap& address_map, const uint64_t pc, const uint64_t address_mask) {
    ++address_map[pc & address_mask].reads;
}

inline void printAddressMap(AddressMap& address_map, const uint64_t min_accesses, OutputFileStream& output_file, int COLUMN_WIDTH) {
    static uint64_t seq_id_1st = 0, stream_id = 0;

    for(const auto& p: address_map) {
        const auto reads = p.second.reads;
        const auto writes = p.second.writes;
        const auto total = reads + writes;
        if(total < min_accesses) {
            continue;
        }
        if(STF_EXPECT_FALSE(output_file.isStdout())) {
            stf::format_utils::formatVA(output_file, p.first);
            stf::format_utils::formatSpaces(output_file, static_cast<size_t>(COLUMN_WIDTH) - 16); // uint64_t requires 16 digits of HEX
        } else {
            COLUMN_WIDTH += 4; // random natural number
            stf::format_utils::formatDecLeft(output_file, p.first, COLUMN_WIDTH);
        }
        stf::format_utils::formatDecLeft(output_file, reads, COLUMN_WIDTH);
        stf::format_utils::formatDecLeft(output_file, writes, COLUMN_WIDTH);
        stf::format_utils::formatDecLeft(output_file, total, COLUMN_WIDTH);
        stf::format_utils::formatDecLeft(output_file, seq_id_1st, COLUMN_WIDTH);
        stf::format_utils::formatDecLeft(output_file, stream_id++, COLUMN_WIDTH);
        output_file << "\n";
        seq_id_1st += total;
    }
}

int main(int argc, char** argv) {
    std::string trace_filename;
    std::string output_filename = "-";
    uint64_t alignment = 1;
    uint64_t min_accesses = 0;
    bool user_mode_only = false;
    bool include_instruction_pc = false;
    bool only_instruction_pc = false;
    bool exclude_reads = false, exclude_writes = false;
    uint64_t max_distance_access = std::numeric_limits<uint64_t>::max(), max_distance_stream = std::numeric_limits<uint64_t>::max();
    std::string wkld_id, wkld_name;

    try {
        parseCommandLine(argc,
                         argv,
                         trace_filename,
                         output_filename,
                         alignment,
                         min_accesses,
                         user_mode_only,
                         include_instruction_pc,
                         only_instruction_pc
                         , exclude_reads, exclude_writes
                         , max_distance_access, max_distance_stream
                         );
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }
    const auto batch = (output_filename == "batch");
    if(STF_EXPECT_TRUE(batch)) {
        std::regex rex(R"foo((wkld\.([0-9]+))\.(\S+)\.zstf)foo");
        std::smatch result;
        stf_assert(std::regex_search(trace_filename, result, rex), "can't find wkld ID in " << trace_filename)
        std::stringstream ss;
        ss << result[1] << ".log";
        output_filename = ss.str();
        std::copy(result[2].first, result[2].second, std::back_inserter(wkld_id));
        std::copy(result[3].first, result[3].second, std::back_inserter(wkld_name));
    }

    OutputFileStream output_file(output_filename);
    constexpr int COLUMN_WIDTH = 20; // uint64_t requires 20 digits of DEC
    enum Columns {
        ADDR,
        RD, WR, SUM,
        ACCESS_ID, STREAM_ID,
        PREV_ACCESS_ID,
        OUT_END = PREV_ACCESS_ID,
        DIST_ACCESS,
        PREV_ADDRESS_ID, DIST_STREAM,
        END};
    const std::vector<std::string> columns {"Address"
                                          ,"Reads", "Writes", "Total"
                                          ,"SeqId1st", "StrmId"
                                          ,"PrevSeqId","DistSeq"
                                          ,"PrevStrmId", "DistStrm"
                                          };
    if(STF_EXPECT_FALSE(output_file.isStdout())) {
        for(size_t i = Columns::ADDR; i != Columns::OUT_END; ++i) {
            stf::format_utils::formatLeft(output_file, columns.at(i).c_str(), COLUMN_WIDTH);
        }
        output_file << "\n";
    }
    AddressMap address_map;
    const uint64_t address_mask = std::numeric_limits<uint64_t>::max() << log2(alignment);

    size_t total_insts = 0;
    if(STF_EXPECT_TRUE(user_mode_only)) {
        stf::STFInstReader reader(trace_filename, user_mode_only);
        for(const auto& inst: reader) {
            ++total_insts;
            if(!only_instruction_pc) {
                for(const auto& access: inst.getMemoryAccesses()) {
                    if (countAddress(address_map, access.getAccessRecord(), address_mask, exclude_reads, exclude_writes) < 0) {
                        printAddressMap(address_map, min_accesses, output_file, COLUMN_WIDTH);
                        address_map.clear();
                        countAddress(address_map, access.getAccessRecord(), address_mask, exclude_reads, exclude_writes);
                    }
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
                const auto desc = rec->getId();
                total_insts += (desc == stf::descriptors::internal::Descriptor::STF_INST_OPCODE16 ||
                                desc == stf::descriptors::internal::Descriptor::STF_INST_OPCODE32);
                if(STF_EXPECT_TRUE(!only_instruction_pc &&
                                    rec->getId() == stf::descriptors::internal::Descriptor::STF_INST_MEM_ACCESS)) {
                    if (countAddress(address_map, rec->as<stf::InstMemAccessRecord>(), address_mask, exclude_reads, exclude_writes) < 0) {
                        printAddressMap(address_map, min_accesses, output_file, COLUMN_WIDTH);
                        address_map.clear();
                        countAddress(address_map, rec->as<stf::InstMemAccessRecord>(), address_mask, exclude_reads, exclude_writes);
                    }
                }
                else if(include_instruction_pc) {
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

    if(!address_map.empty()) {
        printAddressMap(address_map, min_accesses, output_file, COLUMN_WIDTH);
    }
    output_file.close();

    if(!output_file.isStdout()) {
        const auto sorted_filename = output_filename + ".sorted";
        std::stringstream ss_sort;
        ss_sort << "sort -n -k" << Columns::ADDR + 1 << " -k" << Columns::ACCESS_ID + 1 << " -o " << sorted_filename << " " << output_filename;
        std::system(ss_sort.str().c_str());
        std::ifstream ifstrm(sorted_filename);
        stf_assert(ifstrm.is_open(), "failed to open " << sorted_filename)

        const auto tagged_filename = sorted_filename + ".tagged";
        OutputFileStream tagged_file(tagged_filename);
        for(size_t i = Columns::ADDR; i != Columns::END; ++i) {
            stf::format_utils::formatLeft(tagged_file, columns.at(i).c_str(), COLUMN_WIDTH);
        }
        tagged_file << "\n";

        uint64_t prev_addr = std::numeric_limits<uint64_t>::max(), curr_addr;
        uint64_t prev_access_id = 0, prev_accesses = 0, prev_stream_id = 0;
        uint64_t curr_access_id, curr_accesses, curr_stream_id;
        size_t total_accesses = 0, repeated_accesses = 0, distance_access;
        size_t total_streams = 0, repeated_streams = 0, distance_stream;
        for(std::string line; std::getline(ifstrm, line); ) {
            if (line.empty()) {
                continue;
            }
            std::istringstream iss(line);
            std::vector<std::string> strings((std::istream_iterator<std::string>(iss)), std::istream_iterator<std::string>());
            stf_assert(strings.size() == Columns::OUT_END, "wrong fields in file " << sorted_filename << ":" << total_streams)
            total_accesses += (curr_accesses = std::stoul(strings.at(Columns::SUM)));
            ++total_streams;
            stf::format_utils::formatVA(tagged_file, curr_addr = std::stoul(strings.front()));
            stf::format_utils::formatSpaces(tagged_file, 4);
            for (std::size_t i = Columns::RD; i != Columns::OUT_END; ++i) {
                stf::format_utils::formatLeft(tagged_file, strings[i], COLUMN_WIDTH);
            }
            curr_access_id = std::stoul(strings.at(Columns::ACCESS_ID));
            curr_stream_id = std::stoul(strings.at(Columns::STREAM_ID));
            if (prev_addr != curr_addr) {
                prev_addr = curr_addr;
                stf::format_utils::formatDecLeft(tagged_file, -1, COLUMN_WIDTH);
                stf::format_utils::formatDecLeft(tagged_file, 0, COLUMN_WIDTH);
                stf::format_utils::formatDecLeft(tagged_file, -1, COLUMN_WIDTH);
                stf::format_utils::formatDecLeft(tagged_file, 0, COLUMN_WIDTH);
            } else {
                stf::format_utils::formatDecLeft(tagged_file, prev_access_id, COLUMN_WIDTH);
                stf_assert((prev_access_id + prev_accesses) < curr_access_id, "wrong order for duplicated address in file " << sorted_filename << ":" << total_streams)
                distance_access = curr_access_id + 1 - prev_access_id - prev_accesses;
                stf::format_utils::formatDecLeft(tagged_file, distance_access, COLUMN_WIDTH);

                stf::format_utils::formatDecLeft(tagged_file, prev_stream_id, COLUMN_WIDTH);
                stf_assert(prev_stream_id < curr_stream_id, "wrong order for duplicated address in file " << sorted_filename << ":" << total_streams)
                distance_stream = curr_stream_id - prev_stream_id;
                stf::format_utils::formatDecLeft(tagged_file, distance_stream, COLUMN_WIDTH);

                if (distance_access <= max_distance_access && distance_stream <= max_distance_stream) {
                    repeated_accesses += curr_accesses;
                    ++repeated_streams;
                }
            }
            tagged_file << "\n";
            prev_access_id = curr_access_id;
            prev_accesses = curr_accesses;
            prev_stream_id = curr_stream_id;
        }
        tagged_file.close();

        const auto summary_filename = output_filename + ".summary";
        OutputFileStream summary_file(summary_filename);
        if(STF_EXPECT_FALSE(!batch)) {
            summary_file << "wkldID" << ",wkldName"
                         << ",totalInsts"
                         << ",totalAccesses"
                         << ",repeatedAccesses,rptAPKAccesses,rptAPKI"
                         << ",totalStreams,strmsPKA,strmsPKI"
                         << ",repeatedStreams,rptSPKA,rptSPKI"
                         << "\n";
        }
        summary_file << wkld_id << "," << wkld_name
              << "," << total_insts
              << "," << total_accesses
              << "," << repeated_accesses
              << ",";
        stf::format_utils::formatFloat(summary_file, 1000.0 * static_cast<double>(repeated_accesses) / static_cast<double>(total_accesses), 0, 3);
        summary_file << ",";
        stf::format_utils::formatFloat(summary_file, 1000.0 * static_cast<double>(repeated_accesses) / static_cast<double>(total_insts), 0, 3);

        summary_file << ",";
        stf::format_utils::formatDecLeft(summary_file, total_streams);
        summary_file << ",";
        stf::format_utils::formatFloat(summary_file, 1000.0 * static_cast<double>(total_streams) / static_cast<double>(total_accesses), 0, 3);
        summary_file << ",";
        stf::format_utils::formatFloat(summary_file, 1000.0 * static_cast<double>(total_streams) / static_cast<double>(total_insts), 0, 3);

        summary_file << ",";
        stf::format_utils::formatDecLeft(summary_file, repeated_streams);
        summary_file << ",";
        stf::format_utils::formatFloat(summary_file, 1000.0 * static_cast<double>(repeated_streams) / static_cast<double>(total_accesses), 0, 3);
        summary_file << ",";
        stf::format_utils::formatFloat(summary_file, 1000.0 * static_cast<double>(repeated_streams) / static_cast<double>(total_insts), 0, 3);
        summary_file << "\n";

        summary_file.close();
    }

    return 0;
}
