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

static void parseCommandLine(int argc,
                             char** argv,
                             std::string& infile,
                             std::string& outfile,
                             bool& overwrite,
                             int& compression_level,
                             size_t& chunk_size) {
    overwrite = false;
    compression_level = -1; // -1 == default compression level
    chunk_size = stf::STFWriter::DEFAULT_CHUNK_SIZE;

    trace_tools::CommandLineParser parser("stf_recompress");
    parser.addFlag('f', "Overwrite existing file");
    parser.addFlag('c', "#", "Compression level (ZSTD: 1-22, default 3)");
    parser.addFlag('C', "#", "Chunk size (default " + std::to_string(stf::STFWriter::DEFAULT_CHUNK_SIZE) + ")");
    parser.addPositionalArgument("infile", "STF to recompress");
    parser.addPositionalArgument("outfile", "Output STF file");
    parser.parseArguments(argc, argv);

    overwrite = parser.hasArgument('f');
    parser.getArgumentValue('c', compression_level);
    parser.getArgumentValue('C', chunk_size);

    parser.getPositionalArgument(0, infile);
    parser.getPositionalArgument(1, outfile);
}

int main(int argc, char* argv[]) {
    bool overwrite = false;
    std::string infile;
    std::string outfile;
    int compression_level = -1;
    size_t chunk_size;

    try {
        parseCommandLine(argc, argv, infile, outfile, overwrite, compression_level, chunk_size);
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
    stf::STFWriter writer(outfile_man.getOutputName(), compression_level, chunk_size);

    reader.copyHeader(writer);
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
