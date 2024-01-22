#include <iostream>
#include <map>
#include <vector>

#include "format_utils.hpp"
#include "stf_inst_reader.hpp"

#include "file_utils.hpp"
#include "command_line_parser.hpp"
#include "stf_decoder.hpp"

/**
 * Parses command line options
 * \param argc argc from main
 * \param argv argv from main
 * \param trace_filename Trace to open
 * \param output_filename Destination file for tuple
 */
void parseCommandLine(int argc, char** argv, std::string& trace_filename, std::string& output_filename) {
    trace_tools::CommandLineParser parser("stf_tuple");

    parser.addFlag('o', "output", "output file (defaults to stdout)");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);
    parser.getPositionalArgument(0, trace_filename);
}

/**
 * Formats an tuple count to an std::ostream
 * \param os ostream to use
 * \param category instruction category
 * \param count category count
 * \param total_insts total number of instructions
 * \param column_width formatted column width
 */
inline void formatIMixEntry(OutputFileStream& os,
                            const std::string_view category,
                            const uint64_t count,
                            const uint64_t total_insts,
                            const int column_width) {
    static constexpr int NUM_DECIMAL_PLACES = 2;
    stf::format_utils::formatLeft(os, category, column_width);
    stf::format_utils::formatLeft(os, count, column_width);
    const auto frac = static_cast<double>(count) / static_cast<double>(total_insts);
    stf::format_utils::formatPercent(os, frac, 0, NUM_DECIMAL_PLACES);
    os << std::endl;
}

using StfInstIt = stf::STFInstReader::iterator;

constexpr
bool isLink(uint32_t reg) {
    return (reg == 1) || (reg == 5);
}

int main(int argc, char** argv) {
    std::string output_filename = "-";
    std::string trace_filename;

    try {
        parseCommandLine(argc, argv, trace_filename, output_filename);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    OutputFileStream output_file(output_filename);

    stf::STFInstReader reader(trace_filename);
    stf::STFDecoder decoder0(reader.getInitialIEM());
    stf::STFDecoder decoder1(reader.getInitialIEM());

    uint64_t auipc_jalr_jump{0};          // AUIPC/JALR jump (long direct)
    uint64_t auipc_jalr_return{0};        // AUIPC/JALR return (long direct return does not really make sense)
    uint64_t auipc_jalr_call{0};          // AUIPC/JALR call (long direct)
    uint64_t auipc_jalr_return_call{0};   // AUIPC/JALR return/call (long direct return does not really make sense)
    uint64_t lui_jalr{0};                 // We could break these out but we don't bother since they don't occur in workloads
    uint64_t auipc{0};                    // AUIPC not part of AUIPC/JALR pair
    uint64_t lui{0};                      // LUI not part of LUI/JALR pair
    uint64_t jalr_jump{0};                // JALR jump (indirect)
    uint64_t jalr_return{0};              // JALR return (indirect)
    uint64_t jalr_call{0};                // JALR call (indirect)
    uint64_t jalr_return_call{0};         // JALR return/call (indirect)
    uint64_t jal_jump{0};                 // JAL jump (direct)
    uint64_t jal_call{0};                 // JAL call (direct)
    uint64_t inst_count{0};

    static constexpr int COLUMN_WIDTH = 24;
    static constexpr auto RD  = mavis::InstMetaData::OperandFieldID::RD;
    static constexpr auto RS1 = mavis::InstMetaData::OperandFieldID::RS1;

    // Collect the counts
    StfInstIt itr0 = reader.begin();
    while (itr0 != reader.end()) {
        StfInstIt itr1 = itr0;
        ++itr1;
        if (itr1 == reader.end()) {
            break;
        }

        decoder0.decode(itr0->opcode());
        decoder1.decode(itr1->opcode());

        // Detect long direct relative branch comprised of auipc/jalr pairs where auipc.rd == jalr.rs1
        if (decoder0.isAuipc() && decoder1.isJalr() &&
            (decoder0.getDestRegister(RD) == decoder1.getSourceRegister(RS1))) {
            uint32_t rs1 = decoder1.getSourceRegister(RS1);
            uint32_t rd  = decoder1.getDestRegister(RD);
            bool rd_link = isLink(rd);
            bool rs1_link = isLink(rs1);
            if (!rd_link) {
                if (!rs1_link) {
                    ++auipc_jalr_jump;
                } else {
                    ++auipc_jalr_return;
                }
            } else {
                if (!rs1_link) {
                    ++auipc_jalr_call;
                } else if (rs1 != rd) {
                    ++auipc_jalr_return_call;
                } else {
                    ++auipc_jalr_call;
                }
            }
            ++itr0;
            ++inst_count;
        }

        // Detect long direct absolute branch comprised of lui/jalr pairs where lui.rd == jalr.rs1
        else if (decoder0.isLui() && decoder1.isJalr() &&
                 (decoder0.getDestRegister(RD) == decoder1.getSourceRegister(RS1))) {
            ++lui_jalr;
            ++itr0;
            ++inst_count;
        }

        // Detect single auipc
        else if (decoder0.isAuipc()) {
            ++auipc;
        }

        // Detect single auipc
        else if (decoder0.isLui()) {
            ++lui;
        }

        // Detect single jalr
        else if (decoder0.isJalr()) {
            uint32_t rs1 = decoder0.getSourceRegister(RS1);
            uint32_t rd  = decoder0.getDestRegister(RD);
            bool rd_link = isLink(rd);
            bool rs1_link = isLink(rs1);
            if (!rd_link) {
                if (!rs1_link) {
                    ++jalr_jump;
                } else {
                    ++jalr_return;
                }
            } else {
                if (!rs1_link) {
                    ++jalr_call;
                } else if (rs1 != rd) {
                    ++jalr_return_call;
                } else {
                    ++jalr_call;
                }
            }
        }

        // Dectect single jal
        else if (decoder0.isJal()) {
            uint32_t jal_rd  = decoder0.getDestRegister(RD);
            bool rd_link = isLink(jal_rd);
            if (!rd_link) {
                ++jal_jump;
            } else {
                ++jal_call;
            }
        }

        ++itr0;
        ++inst_count;
    }

    // Print the counts
    stf::format_utils::formatLeft(output_file, "Type", COLUMN_WIDTH);
    stf::format_utils::formatLeft(output_file, "Count", COLUMN_WIDTH);
    output_file << "Percent" << std::endl;

    formatIMixEntry(output_file, "auipc_jalr_jump",        auipc_jalr_jump,        inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "auipc_jalr_return",      auipc_jalr_return,      inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "auipc_jalr_call",        auipc_jalr_call,        inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "auipc_jalr_return_call", auipc_jalr_return_call, inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "lui_jalr",               lui_jalr,               inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "auipc",                  auipc,                  inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "lui",                    lui,                    inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "jalr_jump",              jalr_jump,              inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "jalr_return",            jalr_return,            inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "jalr_call",              jalr_call,              inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "jalr_return_call",       jalr_return_call,       inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "jal_jump",               jal_jump,               inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "jal_call",               jal_call,               inst_count, COLUMN_WIDTH);
    formatIMixEntry(output_file, "inst_count",             inst_count,             inst_count, COLUMN_WIDTH);

    return 0;
}
