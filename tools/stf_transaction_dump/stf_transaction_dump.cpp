#include <iostream>

#include "command_line_parser.hpp"
#include "print_utils.hpp"
#include "stf_transaction_reader.hpp"
#include "protocols/tilelink.hpp"

void parseCommandLine (int argc, char **argv, uint64_t& start_transaction, uint64_t& end_transaction, std::string& trace_filename) {
    // Parse options
    trace_tools::CommandLineParser parser("stf_transaction_dump");

    parser.addFlag('s', "N", "start dumping at N-th transaction");
    parser.addFlag('e', "M", "end dumping at M-th transaction");
    parser.addPositionalArgument("trace", "trace in STF format");

    parser.parseArguments(argc, argv);

    parser.getArgumentValue('s', start_transaction);
    parser.getArgumentValue('e', end_transaction);
    parser.getPositionalArgument(0, trace_filename);

    stf_assert(!end_transaction || (end_transaction >= start_transaction),
               "End transaction (" << end_transaction << ") must be greater than or equal to start transaction (" << start_transaction << ')');
}

int main(int argc, char** argv) {
    try {
        uint64_t start_transaction = 0;
        uint64_t end_transaction = 0;
        std::string trace;
        // Get arguments
        parseCommandLine(argc, argv, start_transaction, end_transaction, trace);
        stf::STFTransactionReader reader(trace);

        if(start_transaction || end_transaction) {
            std::cout << "Start Transaction:" << start_transaction;

            if(end_transaction) {
                std::cout << "  End Transaction:" << end_transaction << std::endl;
            }
            else {
                std::cout << std::endl;
            }
        }

        stf::print_utils::printLabel("PROTOCOL");
        std::cout << reader.getProtocolId() << std::endl;

        const auto start_transaction_idx = start_transaction ? start_transaction - 1 : 0;

        for (auto it = reader.begin(start_transaction_idx); it != reader.end(); ++it) {
            std::cout << *it;
            if (STF_EXPECT_FALSE(end_transaction && (it->index() >= end_transaction))) {
                break;
            }
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }
    return 0;
}
