
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

        stf::STFInstReader stf_reader(config.trace_filename);

        AddrMap::iterator amit;

        auto it = config.start_inst ? stf_reader.seekFromBeginning(config.start_inst - 1) : stf_reader.begin();
        const auto end_it = stf_reader.end();

        for (; it != end_it; ++it) {
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

            // masking off the low bits -- fix me
            uint64_t key = (inst.pc() & ~0x1ULL) & config.addr_mask;

            // Search for instruction matches
            amit = config.amap.find(key);

            const auto& mem_accesses = inst.getMemoryAccesses();

            if (amit != config.amap.end()) {
                if (!amit->second.first_index) {
                    amit->second.first_index = index;
                }

                amit->second.last_index = index;
                amit->second.count++;

                if (!config.summary && !config.per_iter) {
                    std::cout << "0x";
                    stf::print_utils::printHex(key, 10);
                    std::cout << ": trace-inst-num " << index;
                    if (config.print_extra) {
                        if (!mem_accesses.empty()) {
                            const auto& first_access = mem_accesses.front();
                            std::cout << " mem 0x";
                            stf::print_utils::printVA(first_access.getAddress());
                            std::cout << " 0x";
                            stf::format_utils::formatData(std::cout, first_access.getData());
                            std::cout << ' ' << first_access.getData();
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
                key = mem_access.getAddress() & config.addr_mask;

                amit = config.mmap.find(key);
                // TODO: implement translation
                //const auto pmit = config.pmap.find(mit->paddr & config.addr_mask);

                bool mem_found = false;

                if (amit != config.mmap.end()) {
                    mem_found = true;

                    if (!amit->second.first_index)
                        amit->second.first_index = index;

                    amit->second.last_index = index;
                    amit->second.count++;

                }
                // TODO: implement tranlsation
                /*else if (pmit != config.pmap.end()) {
                    mem_found = true;

                    if (!pmit->second.first_index)
                        pmit->second.first_index = index;

                    pmit->second.last_index = index;
                    pmit->second.count++;
                }*/

                if (mem_found && !config.summary && !config.per_iter) {
                    stf::print_utils::printVA(mem_access.getAddress());
                    // FIXME: print PA
                    // std::cout << ':';
                    // stf::print_utils::printPA(mem_access.getPA());
                    std::cout << " trace-inst-num " << index << "  ";
                    stf::format_utils::formatData(std::cout, mem_access.getData());
                }
            }

            if (index == config.end_inst) {
                break;
            }
        }

        if (config.summary) {
            config.summarize();
        }

        if (config.per_iter) {
            for (amit = config.amap.begin(); amit != config.amap.end(); amit++) {
                stf_assert(amit->second.count >= 2,
                           "ERROR:  " << argv[0]
                                      << ":  Only found less than one full iteration, using address 0x"
                                      << std::hex << std::setw(16) << amit->first);
                uint64_t delta_insts = amit->second.last_index - amit->second.first_index;
                stf::print_utils::printFloat(static_cast<float>(delta_insts) / static_cast<float>(amit->second.count - 1), 6, 2);
                std::cout << " instructions/iter using 0x";
                stf::print_utils::printVA(amit->first);
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
