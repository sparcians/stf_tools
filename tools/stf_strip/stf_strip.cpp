#include <unordered_set>

#include "stf_record_types.hpp"
#include "stf_reader.hpp"
#include "stf_writer.hpp"

#include "command_line_parser.hpp"
#include "file_utils.hpp"
#include "stf_descriptor_map.hpp"

using DescriptorSet = std::unordered_set<stf::descriptors::internal::Descriptor>;

void parseCommandLine(int argc,
                      char** argv,
                      DescriptorSet& stripped_records,
                      bool& overwrite,
                      int& compression_level,
                      std::string& input_trace,
                      std::string& output_filename) {
    trace_tools::CommandLineParser parser("stf_strip");
    parser.addMultiFlag('r', "type", "Record type to remove. Can be specified multiple times.");
    parser.addFlag('f', "Allow overwriting the output file");
    parser.addFlag('c', "#", "Compression level (ZSTD: 1-22, default 3)");
    parser.addPositionalArgument("trace", "Trace in STF format");
    parser.addPositionalArgument("output", "Output filename");
    parser.appendHelpText("Allowed record types:");

    for(const auto& p: STF_DESCRIPTOR_NAME_MAP) {
        parser.appendHelpText("    " + p.first);
    }

    parser.appendHelpText("\nExample:");
    parser.appendHelpText("    stf_strip -r STF_INST_REG -c 22 input.zstf output.zstf");

    parser.parseArguments(argc, argv);

    for(const auto& arg: parser.getMultipleValueArgument('r')) {
        const auto it = STF_DESCRIPTOR_NAME_MAP.find(arg);

        parser.assertCondition(it != STF_DESCRIPTOR_NAME_MAP.end(), "Invalid record type specified: ", arg);

        stripped_records.insert(it->second);
    }

    parser.assertCondition(!stripped_records.empty(), "No records were specified for removal. Exiting.");

    overwrite = parser.hasArgument('f');
    parser.getArgumentValue('c', compression_level);
    parser.getPositionalArgument(0, input_trace);
    parser.getPositionalArgument(1, output_filename);
}

int main(int argc, char** argv) {
    DescriptorSet stripped_records;
    bool overwrite = false;
    std::string input_trace;
    std::string output_filename;
    int compression_level = -1;

    try {
        parseCommandLine(argc, argv, stripped_records, overwrite, compression_level, input_trace, output_filename);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    OutputFileManager outfile_man(overwrite);

    try {
        outfile_man.open(input_trace, output_filename);
    }
    catch(const OutputFileManager::FileExistsException& e) {
        std::cerr << e.what() << std::endl << "Specify -f if you want to overwrite." << std::endl;
        return 1;
    }

    stf::STFReader reader(input_trace);
    stf::STFWriter writer(outfile_man.getOutputName(), compression_level);

    reader.copyHeader(writer);
    writer.finalizeHeader();

    try {
        stf::STFRecord::UniqueHandle r;
        while(reader) {
            reader >> r;
            if(STF_EXPECT_FALSE(stripped_records.count(r->getId()) != 0)) {
                continue;
            }

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
