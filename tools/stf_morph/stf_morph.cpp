#include "stf_morpher.hpp"

static STFMorpher parseCommandLine(int argc, char **argv) {
    trace_tools::CommandLineParser parser("stf_morph");
    parser.addFlag('o', "trace", "output filename");
    parser.addFlag('s', "N", "start at instruction N");
    parser.addFlag('e', "M", "end at instruction M");
    STFMorpher::addMorphArguments(parser);

    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);

    std::string trace;
    std::string output;
    uint64_t start_inst = 0;
    uint64_t end_inst = 0;

    parser.getArgumentValue('o', output);
    parser.getArgumentValue('s', start_inst);
    parser.getArgumentValue('e', end_inst);

    if(STF_EXPECT_FALSE(output.empty())) {
        parser.raiseErrorWithHelp("No output file specified.");
    }

    parser.getPositionalArgument(0, trace);

    STFMorpher morphs(parser, trace, output, start_inst, end_inst);

    if(STF_EXPECT_FALSE(morphs.empty())) {
        parser.raiseErrorWithHelp("No modifications specified.");
    }

    return morphs;
}

int main(int argc, char** argv) {
    try {
        STFMorpher morphs = parseCommandLine(argc, argv);
        morphs.process();
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
