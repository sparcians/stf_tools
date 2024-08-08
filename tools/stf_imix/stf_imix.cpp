#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include "format_utils.hpp"
#include "stf_inst_reader.hpp"

#include "file_utils.hpp"
#include "command_line_parser.hpp"
#include "mavis_helpers.hpp"
#include "stf_decoder.hpp"
#include "stf_tracepoint_iterator.hpp"

/**
 * Parses command line options
 * \param argc argc from main
 * \param argv argv from main
 * \param trace_filename Trace to open
 * \param output_filename Destination file for imix
 * \param sorted Set to true if output should be sorted
 * \param by_mnemonic Set to true if categorization should be done by mnemonic instead of mavis category
 */
void parseCommandLine(int argc,
                      char** argv,
                      std::string& trace_filename,
                      std::string& output_filename,
                      bool& sorted,
                      bool& by_mnemonic,
                      bool& by_isa_ext,
                      uint64_t& warmup,
                      bool& skip_non_user,
                      uint64_t& run_length,
                      bool& use_tracepoint_roi) {
    trace_tools::CommandLineParser parser("stf_imix");

    parser.addFlag('o', "output", "output file (defaults to stdout)");
    parser.addFlag('s', "sort output");
    parser.addFlag('m', "categorize by mnemonic");
    parser.addFlag('e', "categorize by ISA extension");
    parser.addFlag('w', "warmup", "number of warmup instructions");
    parser.addFlag('u', "only count user-mode instructions");
    parser.addFlag('r', "run_length", "limit to the first run_length instructions (includes warmup). Default is 0, for no limit.");
    parser.addFlag('T', "only count region of interest between tracepoints. If specified, the -w and -r arguments apply to the ROI between tracepoints.");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.setMutuallyExclusive('m', 'e');
    parser.parseArguments(argc, argv);

    parser.getArgumentValue('o', output_filename);
    sorted = parser.hasArgument('s');
    by_mnemonic = parser.hasArgument('m');
    by_isa_ext = parser.hasArgument('e');
    parser.getArgumentValue('w', warmup);
    skip_non_user = parser.hasArgument('u');
    parser.getArgumentValue('r', run_length);
    use_tracepoint_roi = parser.hasArgument('T');
    parser.getPositionalArgument(0, trace_filename);

    if(run_length) {
        parser.assertCondition(run_length > warmup, "run_length must be greater than warmup");
    }
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param category instruction category
 * \param count category count
 * \param total_insts total number of instructions
 */
template<int COLUMN_WIDTH>
inline void formatIMixEntry(OutputFileStream& os,
                            const std::string_view category,
                            const uint64_t count,
                            const double total_insts) {
    static constexpr int NUM_DECIMAL_PLACES = 2;
    stf::format_utils::formatLeft(os, category, COLUMN_WIDTH);
    stf::format_utils::formatLeft(os, count, COLUMN_WIDTH);
    const auto frac = static_cast<double>(count) / total_insts;
    stf::format_utils::formatPercent(os, frac, 0, NUM_DECIMAL_PLACES);
    os << std::endl;
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param count category count
 * \param category instruction category
 * \param total_insts total number of instructions
 */
template<int COLUMN_WIDTH>
inline void formatIMixEntry(OutputFileStream& os,
                            const uint64_t count,
                            const std::string_view category,
                            const double total_insts) {
    formatIMixEntry<COLUMN_WIDTH>(os, count, category, total_insts);
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param category instruction category
 * \param count category count
 * \param total_insts total number of instructions
 */
template<int COLUMN_WIDTH>
inline void formatIMixEntry(OutputFileStream& os,
                            const mavis_helpers::MavisInstTypeArray::enum_t category,
                            const uint64_t count,
                            const double total_insts) {
    formatIMixEntry<COLUMN_WIDTH>(os, mavis_helpers::MavisInstTypeArray::getTypeString(category), count, total_insts);
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param count category count
 * \param category instruction category
 * \param total_insts total number of instructions
 */
template<int COLUMN_WIDTH>
inline void formatIMixEntry(OutputFileStream& os,
                            const uint64_t count,
                            const mavis_helpers::MavisInstTypeArray::enum_t category,
                            const double total_insts) {
    formatIMixEntry<COLUMN_WIDTH>(os, category, count, total_insts);
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param isa_extension ISA extension
 * \param count category count
 * \param total_insts total number of instructions
 */
template<int COLUMN_WIDTH>
inline void formatIMixEntry(OutputFileStream& os,
                            const mavis_helpers::MavisISAExtensionTypeArray::enum_t category,
                            const uint64_t count,
                            const double total_insts) {
    formatIMixEntry<COLUMN_WIDTH>(os,
                                  mavis_helpers::MavisISAExtensionTypeArray::getTypeString(category),
                                  count,
                                  total_insts);
}

/**
 * Formats an instruction mix count to an std::ostream
 * \param os ostream to use
 * \param count category count
 * \param isa_extension ISA extension
 * \param total_insts total number of instructions
 */
template<int COLUMN_WIDTH>
inline void formatIMixEntry(OutputFileStream& os,
                            const uint64_t count,
                            const mavis_helpers::MavisISAExtensionTypeArray::enum_t category,
                            const double total_insts) {
    formatIMixEntry<COLUMN_WIDTH>(os, category, count, total_insts);
}

/**
 * \struct IMixSortComparer
 * \brief Compares imix map iterators by their count values
 */
template<typename IteratorType>
struct IMixSortComparer {
    bool operator() (const IteratorType& lhs, const IteratorType& rhs) {
        return lhs->second < rhs->second;
    }
};

/**
 * \typedef IMixSorter
 * \brief Sorts imix maps by their count values
 */
template<typename MapIterator>
using IMixSorter = std::priority_queue<MapIterator, std::vector<MapIterator>, IMixSortComparer<MapIterator>>;

/**
 * Prints a sorted imix to an output stream
 * \param os output stream to use
 * \param sorter sorted imix counts
 * \param total_insts total # of instructions
 */
template<int COLUMN_WIDTH, typename MapIterator>
inline void formatIMixMap(OutputFileStream& os,
                          IMixSorter<MapIterator>& sorter,
                          const double total_insts) {
    while(!sorter.empty()) {
        const auto& it = sorter.top();
        formatIMixEntry<COLUMN_WIDTH>(os, it->first, it->second, total_insts);
        sorter.pop();
    }
}

/**
 * Prints imix to an output stream
 * \param os output stream to use
 * \param imix_map imix counts
 * \param total_insts total # of instructions
 */
template<int COLUMN_WIDTH, typename MapType>
inline void formatIMixMap(OutputFileStream& os,
                          const MapType& imix_map,
                          const double total_insts) {
    for(const auto& p: imix_map) {
        formatIMixEntry<COLUMN_WIDTH>(os, p.first, p.second, total_insts);
    }
}

/**
 * Prints imix to an output stream, optionally sorting it first
 * \param os output stream to use
 * \param imix_map imix counts
 * \param total_insts total # of instructions
 * \param sorted if true, sort the counts before printing
 */
template<int COLUMN_WIDTH, typename MapType>
void sortAndPrintIMix(OutputFileStream& os,
                      const MapType& imix_map,
                      const double total_insts,
                      const bool sorted) {
    stf::format_utils::formatLeft(os, "Type", COLUMN_WIDTH);
    stf::format_utils::formatLeft(os, "Count", COLUMN_WIDTH);
    os << "Percent" << std::endl;

    if(sorted) {
        // Quick and easy way to sort by instruction counts - copy them into an std::multimap
        // with values and keys swapped
        IMixSorter<typename MapType::const_iterator> sorted_counts;
        for(auto it = imix_map.begin(); it != imix_map.end(); ++it) {
            sorted_counts.push(it);
        }

        formatIMixMap<COLUMN_WIDTH>(os, sorted_counts, total_insts);
    }
    else {
        formatIMixMap<COLUMN_WIDTH>(os, imix_map, total_insts);
    }
}

template<typename MavisEnum>
inline void countMavisEnums(stf::enums::int_t<MavisEnum> packed_types,
                            std::unordered_map<MavisEnum, uint64_t>& count_map,
                            const uint64_t count) {
    // This loop skips over every 0-bit until it finds the first 1 bit, then increments the corresponding
    // category count
    while(packed_types) {
        const decltype(packed_types) type = 1ULL <<  __builtin_ctzl(packed_types);
        count_map[static_cast<MavisEnum>(type)] += count;
        packed_types ^= type; // clear the bit we found
    }
}

template<typename IteratorType>
void processTrace(const std::string& output_filename,
                  const std::string& trace_filename,
                  const bool sorted,
                  const bool by_mnemonic,
                  const bool by_isa_ext,
                  const uint64_t warmup,
                  const bool skip_non_user,
                  const uint64_t run_length) {
    OutputFileStream output_file(output_filename);

    stf::STFInstReader reader(trace_filename, skip_non_user);
    stf::STFDecoder decoder(reader.getInitialIEM());

    static constexpr int COLUMN_WIDTH = 16;

    std::unordered_map<uint32_t, uint64_t> opcode_counts;
    std::unordered_map<std::string, uint64_t> mnemonic_counts;
    std::unordered_map<mavis_helpers::MavisInstTypeArray::enum_t, uint64_t> category_counts;
    std::unordered_map<mavis_helpers::MavisISAExtensionTypeArray::enum_t, uint64_t> isa_extension_counts;

    uint64_t num_insts_read = 0;
    const uint64_t post_warmup_run_length = run_length == 0 ? std::numeric_limits<uint64_t>::max() : run_length - warmup;

    for(auto it = stf::getStartIterator<IteratorType>(reader, decoder, warmup); it != reader.end(); ++it) {
        const auto opcode = it->opcode();

        if(STF_EXPECT_TRUE(!it->isFault())) {
            ++opcode_counts[opcode];
            ++num_insts_read;
        }

        if(STF_EXPECT_FALSE(num_insts_read >= post_warmup_run_length)) {
            break;
        }
    }

    const auto total_insts = static_cast<double>(num_insts_read);

    for(const auto& p: opcode_counts) {
        decoder.decode(p.first);

        if(by_mnemonic) {
            mnemonic_counts[decoder.getMnemonic()] += p.second;
        }
        else if(by_isa_ext) {
            countMavisEnums(decoder.getISAExtensions(), isa_extension_counts, p.second);
        }
        else {
            const auto inst_types = decoder.getInstTypes();
            if(STF_EXPECT_FALSE(inst_types == stf::enums::to_int(mavis_helpers::MavisInstTypeArray::UNDEFINED))) {
                category_counts[mavis_helpers::MavisInstTypeArray::UNDEFINED] += p.second;
            }
            else {
                countMavisEnums(inst_types, category_counts, p.second);
            }
        }
    }

    if(by_mnemonic) {
        sortAndPrintIMix<COLUMN_WIDTH>(output_file, mnemonic_counts, total_insts, sorted);
    }
    else if(by_isa_ext) {
        sortAndPrintIMix<COLUMN_WIDTH>(output_file, isa_extension_counts, total_insts, sorted);
    }
    else {
        sortAndPrintIMix<COLUMN_WIDTH>(output_file, category_counts, total_insts, sorted);
    }
}

int main(int argc, char** argv) {
    std::string output_filename = "-";
    std::string trace_filename;
    bool sorted = false;
    bool by_mnemonic = false;
    bool by_isa_ext = false;
    uint64_t warmup = 0;
    bool skip_non_user = false;
    uint64_t run_length = 0;
    bool use_tracepoint_roi = false;

    try {
        parseCommandLine(argc,
                         argv,
                         trace_filename,
                         output_filename,
                         sorted,
                         by_mnemonic,
                         by_isa_ext,
                         warmup,
                         skip_non_user,
                         run_length,
                         use_tracepoint_roi);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    if(use_tracepoint_roi) {
        processTrace<stf::STFTracepointIterator>(output_filename, trace_filename, sorted, by_mnemonic, by_isa_ext, warmup, skip_non_user, run_length);
    }
    else {
        processTrace<stf::STFInstReader::iterator>(output_filename, trace_filename, sorted, by_mnemonic, by_isa_ext, warmup, skip_non_user, run_length);
    }
    return 0;
}
