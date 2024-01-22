#include <iostream>
#include <string>
#include <cstdint>

#include "print_utils.hpp"
#include "stf_inst_reader.hpp"
#include "command_line_parser.hpp"
#include "dependency_tracker.hpp"
#include "tools_util.hpp"

constexpr uint32_t NUM_BITS = 64;
constexpr uint64_t address_mask_4k = std::numeric_limits<uint64_t>::max() << 12;
constexpr uint64_t address_mask_64b = std::numeric_limits<uint64_t>::max() << 6;
//constexpr uint64_t address_mask_4m = std::numeric_limits<uint64_t>::max() << 22;

class RegionInfo {
public:
    uint32_t accessed = 0;
    std::bitset<NUM_BITS> bit_map;
};

void processCommandLine(int argc,
                        char** argv,
                        std::string& trace,
                        bool& skip_non_user) {
    trace_tools::CommandLineParser parser("stf_ls_access_map");
    parser.addFlag('u', "skip non user-mode instructions");
    parser.addPositionalArgument("trace", "trace in STF format");
    parser.parseArguments(argc, argv);
    skip_non_user = parser.hasArgument('u');
    parser.getPositionalArgument(0, trace);
}

int main(int argc, char** argv) {
    std::string trace;
    bool skip_non_user = false;

    try {
        processCommandLine(argc, argv, trace, skip_non_user);
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }


    std::vector<uint32_t> access_map_4k(NUM_BITS);
    std::set<uint64_t> cache_lines;
    std::map<uint64_t, RegionInfo> regions;

    stf::STFInstReader reader(trace, skip_non_user);

    uint64_t access_total = 0;
    for(const auto& inst: reader) {
        if (inst.isLoad()) {
            for(const auto& m: inst.getMemoryReads()) {
                const uint64_t addr = m.getAddress();
                const uint64_t cache_line = addr & address_mask_64b;
                const uint64_t region = addr & address_mask_4k;
                const uint64_t region_bit = (addr & ~address_mask_4k) >> 6;
                ++access_map_4k[region_bit];
                ++access_total;
                cache_lines.insert(cache_line);
                if (regions.find(region) == regions.end()) {
                    regions.insert(std::pair<uint64_t,RegionInfo>(region, RegionInfo()));
                }
                regions[region].bit_map.set(region_bit);
                ++regions[region].accessed;
                //std::cout << "load: " << m.getAddress() << "\n";
            }
        }
        /*
        if (inst.isStore()) {
            for(const auto& m: inst.getMemoryWrites()) {
                std::cout << "store: " << m.getAddress() << "\n";
            }
        }
        */
    }

    std::cout << "Total accesses=" << access_total << std::endl;
    std::cout << "Unique cache lines=" << cache_lines.size() << std::endl;
    std::cout << "Unique 4k regions=" << regions.size() << std::endl;
    std::cout << "access/cacheline=" << static_cast<float>(access_total) / static_cast<float>(cache_lines.size()) << std::endl;
    std::cout << "access/region=" << static_cast<float>(access_total) / static_cast<float>(regions.size()) << std::endl;
    std::cout << "cacheline/region=" << static_cast<float>(cache_lines.size()) / static_cast<float>(regions.size()) << std::endl;
    for (const auto& region : regions) {
        stf::print_utils::printHex(region.first);
        std::cout << "access=";
        stf::print_utils::printDec(region.second.accessed, 7, ' ');
        std::cout << ' '
                  << region.second.bit_map.to_string<char,std::string::traits_type,std::string::allocator_type>()
                  << std::endl;
    }
    for (uint32_t bit = 0; bit < NUM_BITS; ++bit) {
        std::cout << '[';
        stf::print_utils::printDec(bit, 2, ' ');
        std::cout << "]=";
        stf::print_utils::printDec(access_map_4k[bit], 7, ' ');
        std::cout << ' ';
        stf::print_utils::printPercent(static_cast<float>(access_map_4k[bit]) / static_cast<float>(access_total), 1, 2);
        std::cout << std::endl;
    }

    return 0;
}
