
// <STF_imem> -*- C++ -*-

/**
 * \brief  This tool finds instructions based on various criteria, and prints
 * 	   out what trace-inst-num they are at.
 *
 */

#include <set>
#include <vector>
#include <iomanip>

#include "stf_inst_reader.hpp"
#include "stf_find.hpp"

int main (int argc, char **argv)
{
    try {
        STFFindConfig config(argc, argv);

        stf::STFInstReader stf_reader(config.trace_filename, config.skip_non_user);

        const auto start_inst = config.start_inst ? (config.start_inst - 1) : 0;

        uint64_t num_matches = 0;
        for (auto it = stf_reader.begin(start_inst); it != stf_reader.end(); ++it) {
            bool found_match = false;
            const auto& inst = *it;

            if (!inst.valid()) {
                std::cerr << "ERROR: ";
                stf::format_utils::formatDec(std::cerr, inst.index());
                std::cerr << " invalid instruction ";
                stf::format_utils::formatHex(std::cerr, inst.opcode());
                std::cerr << " PC ";
                stf::format_utils::formatHex(std::cerr, inst.pc());
                std::cerr << std::endl;
            }

            uint64_t index = inst.index();

            const auto& mem_accesses = inst.getMemoryAccesses();

            // Search for instruction matches
            // masking off the low bits -- fix me
            const uint64_t key = inst.pc() & ~0x1ULL;

            if (const auto it = config.findAddress(config.amap, inst.pc()); it != config.amap.end()) {
                if (!it->second.first_index) {
                    it->second.first_index = index;
                }

                it->second.last_index = index;
                it->second.count++;

                found_match = true;

                if (!config.summary && !config.per_iter) {
                    std::cout << "0x";
                    stf::print_utils::printHex(key, 10);
                    std::cout << ": trace-inst-num " << index;
                    if (config.print_extra) {
                        if (!mem_accesses.empty()) {
                            const auto& first_access = mem_accesses.front();
                            std::cout << " mem 0x";
                            stf::print_utils::printVA(first_access.getAddress());
                            first_access.formatContent<true, true>(std::cout);
                        }
                        if (inst.isTakenBranch()) {
                            std::cout << " tgt 0x";
                            stf::print_utils::printVA(inst.branchTarget());
                        }
                    }
                    std::cout << std::endl;
                }
            }

            for (const auto& mem_access: mem_accesses) {
                bool mem_found = false;

                // Search for VA
                if (const auto it = config.findAddress(config.mmap, mem_access.getAddress()); it != config.mmap.end()) {
                    mem_found = true;

                    if (!it->second.first_index)
                        it->second.first_index = index;

                    it->second.last_index = index;
                    it->second.count++;

                }
                // Search for PA
                else if (const auto it = config.findAddress(config.pmap, mem_access.getPhysAddress()); it != config.pmap.end()) {
                    mem_found = true;

                    if (!it->second.first_index)
                        it->second.first_index = index;

                    it->second.last_index = index;
                    it->second.count++;
                }

                found_match |= mem_found;

                if (mem_found && !config.summary && !config.per_iter) {
                    stf::print_utils::printVA(mem_access.getAddress());
                    // FIXME: print PA
                    // std::cout << ':';
                    // stf::print_utils::printPA(mem_access.getPA());
                    std::cout << " trace-inst-num " << index << ' ';
                    mem_access.formatContent<true, true>(std::cout);
                    std::cout << std::endl;
                }
            }

            num_matches += found_match;

            if (index == config.end_inst || num_matches == config.max_matches) {
                break;
            }
        }

        if (config.summary) {
            config.summarize();
        }

        if (config.per_iter) {
            for (auto it = config.amap.begin(); it != config.amap.end(); ++it) {
                stf_assert(it->second.count >= 2,
                           "ERROR:  " << argv[0]
                                      << ":  Only found less than one full iteration, using address 0x"
                                      << std::hex << std::setw(16) << it->first);
                uint64_t delta_insts = it->second.last_index - it->second.first_index;
                stf::print_utils::printFloat(static_cast<float>(delta_insts) / static_cast<float>(it->second.count - 1), 6, 2);
                std::cout << " instructions/iter using 0x";
                stf::print_utils::printVA(it->first);
                std::cout << std::endl;
            }
        }
    }
    catch(const trace_tools::CommandLineParser::EarlyExitException& e) {
        std::cerr << e.what() << std::endl;
        return e.getCode();
    }

    return 0;
}
