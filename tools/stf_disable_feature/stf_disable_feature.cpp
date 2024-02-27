#include <algorithm>
#include <cstdlib>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "command_line_parser.hpp"
#include "file_utils.hpp"
#include "stf_record_types.hpp"
#include "stf_reader.hpp"
#include "stf_writer.hpp"
#include "tools_util.hpp"

#define PARSER_ENTRY(x) { #x, stf::TRACE_FEATURES::x }

static stf::TRACE_FEATURES inline parseFeature(const std::string_view feature_str) {
    static const std::unordered_map<std::string_view, stf::TRACE_FEATURES> string_map {
        PARSER_ENTRY(STF_CONTAIN_PHYSICAL_ADDRESS),
        PARSER_ENTRY(STF_CONTAIN_DATA_ATTRIBUTE),
        PARSER_ENTRY(STF_CONTAIN_OPERAND_VALUE),
        PARSER_ENTRY(STF_CONTAIN_EVENT),
        PARSER_ENTRY(STF_CONTAIN_SYSTEMCALL_VALUE),
        PARSER_ENTRY(STF_CONTAIN_RV64),
        PARSER_ENTRY(STF_CONTAIN_INT_DIV_OPERAND_VALUE),
        PARSER_ENTRY(STF_CONTAIN_SAMPLING),
        PARSER_ENTRY(STF_CONTAIN_PTE),
        PARSER_ENTRY(STF_CONTAIN_SIMPOINT),
        PARSER_ENTRY(STF_CONTAIN_PROCESS_ID),
        PARSER_ENTRY(STF_CONTAIN_PTE_ONLY),
        PARSER_ENTRY(STF_NEED_POST_PROCESS),
        PARSER_ENTRY(STF_CONTAIN_REG_STATE),
        PARSER_ENTRY(STF_CONTAIN_MICROOP),
        PARSER_ENTRY(STF_CONTAIN_MULTI_THREAD),
        PARSER_ENTRY(STF_CONTAIN_MULTI_CORE),
        PARSER_ENTRY(STF_CONTAIN_PTE_HW_AD),
        PARSER_ENTRY(STF_CONTAIN_VEC),
        PARSER_ENTRY(STF_CONTAIN_EVENT64),
        PARSER_ENTRY(STF_CONTAIN_TRANSACTIONS)
    };

    const auto it = string_map.find(feature_str);
    stf_assert(it != string_map.end(), "Invalid feature specified: " << feature_str);
    return it->second;
}

static void parseCommandLine(int argc,
                             char** argv,
                             std::string& infile,
                             std::string& outfile,
                             std::vector<stf::TRACE_FEATURES>& features,
                             bool& overwrite) {
    overwrite = false;

    trace_tools::CommandLineParser parser("stf_recompress");
    parser.addFlag('f', "Overwrite existing file");
    parser.addPositionalArgument("infile", "STF to recompress");
    parser.addPositionalArgument("outfile", "Output STF file");
    parser.addPositionalArgument("features", "Feature(s) to disable", true);

    parser.parseArguments(argc, argv);

    overwrite = parser.hasArgument('f');

    parser.getPositionalArgument(0, infile);
    parser.getPositionalArgument(1, outfile);

    const auto& feature_args = parser.getMultipleValuePositionalArgument(2);
    std::transform(feature_args.begin(),
                   feature_args.end(),
                   std::back_inserter(features),
                   [](const auto& feature) { return parseFeature(feature); });
}

int main(int argc, char* argv[]) {
    bool overwrite = false;
    std::string infile;
    std::string outfile;
    std::vector<stf::TRACE_FEATURES> features;

    try {
        parseCommandLine(argc, argv, infile, outfile, features, overwrite);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    OutputFileManager outfile_man(overwrite);

    try {
        outfile_man.open(infile, outfile);
    }
    catch(const OutputFileManager::FileExistsException& e) {
        std::cerr << e.what() << std::endl << "Specify -f if you want to overwrite." << std::endl;
        return 1;
    }

    stf::STFReader reader(infile);
    stf::STFWriter writer(outfile_man.getOutputName());

    reader.copyHeader(writer);
    for(const auto feature: features) {
        writer.disableTraceFeature(feature);
    }
    writer.finalizeHeader();

    try {
        stf::STFRecord::UniqueHandle r;
        while(reader) {
            reader >> r;
            writer << *r;
        }
    }
    catch(const stf::EOFException&) {
    }

    reader.close();
    writer.close();

    outfile_man.setSuccess();

    return 0;
}
