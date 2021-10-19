#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <cxxabi.h>

#include "command_line_parser.hpp"
#include "stf_branch_reader.hpp"
#include "stf_inst_reader.hpp"

std::string demangle(const char* name) {

    int status = -4; // some arbitrary value to eliminate the compiler warning

    // enable c++11 by passing the flag -std=c++11 to g++
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };

    return (status==0) ? res.get() : name ;
}

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

template<typename Reader>
void readerBench(const std::string& filename) {
    Reader reader(filename);

    auto time = readAllRecords<Reader>(reader);

    std::cout << demangle(typeid(Reader).name())
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
        parser.addPositionalArgument("trace", "STF to test with");
        parser.parseArguments(argc, argv);

        parser.getArgumentValue('r', reader);
        const auto trace = parser.getPositionalArgument<std::string>(0);

        switch(reader) {
            case 0:
                readerBench<stf::STFReader>(trace);
                readerBench<stf::STFInstReader>(trace);
                readerBench<stf::STFBranchReader>(trace);
                break;
            case 1:
                readerBench<stf::STFReader>(trace);
                break;
            case 2:
                readerBench<stf::STFInstReader>(trace);
                break;
            case 3:
                readerBench<stf::STFBranchReader>(trace);
                break;
        };
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
