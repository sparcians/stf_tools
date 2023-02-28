#pragma once

#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <string>

#include "command_line_parser.hpp"
#include "print_utils.hpp"
#include "stf.hpp"
#include "stf_exception.hpp"
#include "tools_util.hpp"

//#define DEBUG

struct Stat {
    uint64_t    first_index = 0;
    uint64_t    last_index = 0;
    uint64_t    count = 0;
};

using AddrMap = std::map<uint64_t, Stat>;

class STFFindConfig {
    private:
        static void validateAddr_(const uint64_t addr, const uint64_t mask) {
            stf_assert((addr & (~mask)) == 0,
                       "ERROR: address " << std::hex << addr << " does not work with mask " << mask);
        }

        static void printAddrMap_(const AddrMap& m) {
            for (const auto& a: m) {
                std::cout << "0x";
                stf::print_utils::printVA(a.first);
                std::cout << ": first trace-inst-num is "
                          << a.second.first_index
                          << ", last is "
                          << a.second.last_index
                          << ", total "
                          << a.second.count
                          << " occurrences."
                          << std::endl;
            }
        }

    public:
        AddrMap amap;
        AddrMap mmap;
        AddrMap pmap;
        uint64_t addr_mask = std::numeric_limits<uint64_t>::max();
        uint64_t start_inst = 0;
        uint64_t end_inst = 0;
        uint64_t max_matches = std::numeric_limits<uint64_t>::max();
        bool skip_non_user = false;
        bool summary = false;
        bool per_iter = false;
        bool print_extra = false;
        std::string trace_filename;

        void summarize() const {
            printAddrMap_(amap);
            printAddrMap_(mmap);
            printAddrMap_(pmap);
        }

        /**
         * \brief Parse the command line options
         *
         */
        STFFindConfig(int argc, char **argv) {
            // Parse options
            trace_tools::CommandLineParser parser("stf_find");
            parser.addFlag('u', "skip non-user instructions");
            parser.addMultiFlag('a', "addr", "find instructions at the specified address");
            parser.addMultiFlag('m', "addr", "find memory accesses at this address");
            parser.addMultiFlag('p', "addr", "find memory accesses at this physical address");
            parser.addFlag('M', "mask", "mask to apply to memory accesses");
        	parser.addFlag('e', "print extra info (e.g., memory address/data for load/store).");
        	parser.addFlag('s', "summary:  show first and last tin, and total count.");
            parser.addFlag('S', "inst", "skip to instruction # <inst>");
            parser.addFlag('E', "inst", "stop after instruction # <inst>");
            parser.addFlag('I', "per-Iter:  show instructions between these markers, per iter, concisely.");
            parser.addFlag('c', "count", "stop after <count> matches");
            parser.addPositionalArgument("trace", "trace in STF format");
            parser.parseArguments(argc, argv);

            for(const auto& arg: parser.getMultipleValueArgument('a')) {
                amap.emplace(std::piecewise_construct,
                             std::forward_as_tuple(parseHex<uint64_t>(arg)),
                             std::forward_as_tuple());
            }

            for(const auto& arg: parser.getMultipleValueArgument('m')) {
                mmap.emplace(std::piecewise_construct,
                             std::forward_as_tuple(parseHex<uint64_t>(arg)),
                             std::forward_as_tuple());
            }

            for(const auto& arg: parser.getMultipleValueArgument('p')) {
                pmap.emplace(std::piecewise_construct,
                             std::forward_as_tuple(parseHex<uint64_t>(arg)),
                             std::forward_as_tuple());
            }

            parser.getArgumentValue<uint64_t, 16>('M', addr_mask);
            parser.getArgumentValue('S', start_inst);
            parser.getArgumentValue('E', end_inst);
            parser.getArgumentValue('c', max_matches);
            print_extra = parser.hasArgument('e');
            skip_non_user = parser.hasArgument('u');
            summary = parser.hasArgument('s');
            per_iter = parser.hasArgument('I');

            parser.getPositionalArgument(0, trace_filename);

            if (print_extra && summary) {
                std::cout << "WARN: -e is suppressed when '-s' is used" << std::endl;
            }

            for (auto const& elem: amap) {
                validateAddr_(elem.first, addr_mask);
            }
            for (auto const& elem: mmap) {
                validateAddr_(elem.first, addr_mask);
            }
            for (auto const& elem: pmap) {
                validateAddr_(elem.first, addr_mask);
            }
        }
};
