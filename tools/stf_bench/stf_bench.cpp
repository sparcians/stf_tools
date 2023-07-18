#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include <boost/core/demangle.hpp>

#include "command_line_parser.hpp"
#include "stf_branch_reader.hpp"
#include "stf_inst_reader.hpp"

template<typename Reader>
inline std::chrono::duration<double> readAllRecords(Reader& reader) {
    auto start = std::chrono::system_clock::now();
    for(const auto& rec: reader) {
        (void)rec;
    }
    auto end = std::chrono::system_clock::now();
    return end - start;
}

template<>
inline std::chrono::duration<double> readAllRecords(stf::STFReader& reader) {
    stf::STFRecord::UniqueHandle rec;
    auto start = std::chrono::system_clock::now();
    try {
        while(reader >> rec) {
            (void)rec;
        }
    }
    catch(const stf::EOFException&) {
    }
    auto end = std::chrono::system_clock::now();
    return end - start;
}

template<typename Reader, typename ... Args>
void readerBench(const std::string& filename, Args&&... args) {
    Reader reader(filename, args...);

    auto time = readAllRecords<Reader>(reader);

    std::cout << boost::core::demangle(typeid(Reader).name())
              << std::endl
              << "Read "
              << reader.numRecordsRead()
              << " records in "
              << time.count()
              << " seconds ("
              << (static_cast<double>(reader.numRecordsRead()) / time.count())
              << " rec/s)" << std::endl
              << reader.numInstsRead()
              << " instructions ("
              << (static_cast<double>(reader.numInstsRead()) / time.count())
              << " insts/s)" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        int reader = 0;

        trace_tools::CommandLineParser parser("stf_bench");
        parser.addFlag('r', "reader", "Reader to test (0 = all, 1 = STFReader, 2 = STFInstReader, 3 = STFBranchReader)");
        parser.addFlag('u', "Skip non-user instructions (will not apply to STFReader)");
        parser.addFlag('p', "Enable page table tracking");
        parser.addPositionalArgument("trace", "STF to test with");
        parser.parseArguments(argc, argv);

        const bool skip_non_user = parser.hasArgument('u');
        const bool track_page_table_entries = parser.hasArgument('p');
        parser.getArgumentValue('r', reader);
        const auto trace = parser.getPositionalArgument<std::string>(0);

        switch(reader) {
            case 0:
                readerBench<stf::STFReader>(trace);
                readerBench<stf::STFInstReader>(trace, skip_non_user, track_page_table_entries);
                readerBench<stf::STFBranchReader>(trace, skip_non_user);
                break;
            case 1:
                readerBench<stf::STFReader>(trace);
                break;
            case 2:
                readerBench<stf::STFInstReader>(trace, skip_non_user, track_page_table_entries);
                break;
            case 3:
                readerBench<stf::STFBranchReader>(trace, skip_non_user);
                break;
        };
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
