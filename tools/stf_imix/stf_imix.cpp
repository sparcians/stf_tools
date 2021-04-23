#include <iostream>
#include <map>
#include <vector>

#include "format_utils.hpp"
#include "stf_inst_reader.hpp"

#include "file_utils.hpp"
#include "command_line_parser.hpp"
#include "mavis_helpers.hpp"
#include "stf_decoder.hpp"

/**
 * Parses command line options
 * \param argc argc from main
 * \param argv argv from main
 * \param trace_filename Trace to open
 * \param output_filename Destination file for imix
 * \param sorted Set to true if output should be sorted
 * \param by_mnemonic Set to true if categorization should be done by mnemonic instead of mavis category
 */
void parseCommandLine(int argc, char** argv, std::string& trace_filename, std::string& output_filename, bool& sorted, bool& by_mnemonic) {
    trace_tools::CommandLineParser parser("stf_imix");

    parser.addFlag('o', "output", "output file (defaults to stdout)");
    parser.addFlag('s', "sort output");
    parser.addFlag('m', "categorize by mnemonic");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);
    sorted = parser.hasArgument('s');
    by_mnemonic = parser.hasArgument('m');
    parser.getPositionalArgument(0, trace_filename);
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param category instruction category
 * \param count category count
 * \param total_insts total number of instructions
 * \param column_width formatted column width
 */
inline void formatIMixEntry(OutputFileStream& os,
                            const std::string_view category,
                            const uint64_t count,
                            const double total_insts,
                            const int column_width) {
    static constexpr int NUM_DECIMAL_PLACES = 2;
    stf::format_utils::formatLeft(os, category, column_width);
    stf::format_utils::formatLeft(os, count, column_width);
    const auto frac = static_cast<double>(count) / total_insts;
    stf::format_utils::formatPercent(os, frac, 0, NUM_DECIMAL_PLACES);
    os << std::endl;
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param category instruction category
 * \param count category count
 * \param total_insts total number of instructions
 * \param column_width formatted column width
 */
inline void formatIMixEntry(OutputFileStream& os,
                            const mavis_helpers::MavisInstTypeArray::enum_t category,
                            const uint64_t count,
                            const double total_insts,
                            const int column_width) {
    formatIMixEntry(os, mavis_helpers::MavisInstTypeArray::getTypeString(category), count, total_insts, column_width);
}

int main(int argc, char** argv) {
    std::string output_filename = "-";
    std::string trace_filename;
    bool sorted = false;
    bool by_mnemonic = false;

    try {
        parseCommandLine(argc, argv, trace_filename, output_filename, sorted, by_mnemonic);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    OutputFileStream output_file(output_filename);

    stf::STFDecoder decoder;
    stf::STFInstReader reader(trace_filename);

    static constexpr int COLUMN_WIDTH = 16;

    if(by_mnemonic) {
        std::map<std::string, uint64_t> imix_counts;
        for(const auto& inst: reader) {
            decoder.decode(inst.opcode());
            ++imix_counts[decoder.getMnemonic()];
        }

        stf::format_utils::formatLeft(output_file, "Type", COLUMN_WIDTH);
        stf::format_utils::formatLeft(output_file, "Count", COLUMN_WIDTH);
        output_file << "Percent" << std::endl;

        const auto total_insts = static_cast<double>(reader.numInstsRead());

        if(sorted) {
            // Quick and easy way to sort by instruction counts - copy them into an std::multimap
            // with values and keys swapped
            std::multimap<uint64_t, std::string> sorted_imix_counts;
            for(const auto& p: imix_counts) {
                sorted_imix_counts.emplace(p.second, p.first);
            }

            for(auto it = sorted_imix_counts.rbegin(); it != sorted_imix_counts.rend(); ++it) {
                formatIMixEntry(output_file, it->second, it->first, total_insts, COLUMN_WIDTH);
            }
        }
        else {
            for(const auto& imix_pair: imix_counts) {
                formatIMixEntry(output_file, imix_pair.first, imix_pair.second, total_insts, COLUMN_WIDTH);
            }
        }
    }
    else {
        std::map<mavis_helpers::MavisInstTypeArray::enum_t, uint64_t> imix_counts;

        for(const auto type: mavis_helpers::MavisInstTypeArray()) {
            imix_counts[type] = 0;
        }

        for(const auto& inst: reader) {
            decoder.decode(inst.opcode());
            for(auto& imix_pair: imix_counts) {
                if(decoder.isInstType(imix_pair.first)) {
                    ++imix_pair.second;
                }
            }
        }

        stf::format_utils::formatLeft(output_file, "Type", COLUMN_WIDTH);
        stf::format_utils::formatLeft(output_file, "Count", COLUMN_WIDTH);
        output_file << "Percent" << std::endl;

        const auto total_insts = static_cast<double>(reader.numInstsRead());

        if(sorted) {
            // Quick and easy way to sort by instruction counts - copy them into an std::multimap
            // with values and keys swapped
            std::multimap<uint64_t, mavis_helpers::MavisInstTypeArray::enum_t> sorted_imix_counts;
            for(const auto& p: imix_counts) {
                sorted_imix_counts.emplace(p.second, p.first);
            }

            for(auto it = sorted_imix_counts.rbegin(); it != sorted_imix_counts.rend(); ++it) {
                formatIMixEntry(output_file, it->second, it->first, total_insts, COLUMN_WIDTH);
            }
        }
        else {
            for(const auto& imix_pair: imix_counts) {
                formatIMixEntry(output_file, imix_pair.first, imix_pair.second, total_insts, COLUMN_WIDTH);
            }
        }
    }

    return 0;
}
