#include <cstdlib>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "command_line_parser.hpp"
#include "file_utils.hpp"
#include "stf_decoder.hpp"
#include "stf_record_types.hpp"
#include "stf_reader.hpp"
#include "stf_writer.hpp"
#include "tools_util.hpp"

static void parseCommandLine(int argc,
                             char** argv,
                             std::string& infile,
                             std::string& outfile,
                             bool& overwrite,
                             std::string& isa_info) {
    overwrite = false;

    trace_tools::CommandLineParser parser("stf_isa_edit");
    parser.addFlag('f', "Overwrite existing file");
    parser.addFlag('i', "isa", "New ISA extended info string");
    parser.addFlag('d', "Delete ISA extended info");
    parser.setMutuallyExclusive('i', 'd');
    parser.addPositionalArgument("infile", "STF to edit");
    parser.addPositionalArgument("outfile", "Output STF file");
    parser.parseArguments(argc, argv);

    const bool has_new_info = parser.hasArgument('i');
    const bool delete_isa_info = parser.hasArgument('d');

    parser.assertCondition(has_new_info || delete_isa_info, "One of the -i or -d arguments must be specified");

    if(has_new_info) {
        parser.getArgumentValue('i', isa_info);
        parser.assertCondition(!isa_info.empty(), "-i argument must not be an empty string");
    }

    overwrite = parser.hasArgument('f');

    parser.getPositionalArgument(0, infile);
    parser.getPositionalArgument(1, outfile);
}

int main(int argc, char* argv[]) {
    bool overwrite = false;
    std::string infile;
    std::string outfile;
    std::string isa_info;

    try {
        parseCommandLine(argc, argv, infile, outfile, overwrite, isa_info);
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

    // Instantiate a decoder with the new ISA string to verify it is valid
    stf::STFDecoderStrict decoder(reader.getISA(), reader.getInitialIEM(), isa_info);

    stf::STFWriter writer(outfile_man.getOutputName());

    reader.copyHeader(writer);
    writer.setISAExtendedInfo(isa_info);

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
