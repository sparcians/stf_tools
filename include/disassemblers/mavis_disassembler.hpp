#pragma once

#include <iostream>
#include "stf_decoder.hpp"
#include "base_disassembler.hpp"

namespace stf {
    namespace disassemblers {
        /**
         * \class MavisDisassembler
         * \brief  A disassembler that uses Mavis as a backend
         *
         */
        class MavisDisassembler : public BaseDisassembler {
            private:
                mutable STFDecoder decoder_;

                /**
                 * \brief Print the disassembly code of an opcode
                 * \param pc PC address of the instruction
                 * \param opcode Opcode of the instruction
                 * \param os The ostream to write the assembly to
                 */
                void printDisassembly_(std::ostream& os,
                                       const uint64_t pc,
                                       const uint32_t opcode) const final {
                    os << decoder_.decode(opcode).getDisassembly();
                }

            public:
                /**
                 * \brief Construct a MavisDisassembler
                 */
                explicit MavisDisassembler(const ISA inst_set, const bool use_aliases) :
                    BaseDisassembler(inst_set, use_aliases)
                {
                }

                ~MavisDisassembler() {
                    if(decoder_.hasUnknownDisasm()) {
                        std::cerr << "One or more unknown instructions were encountered." << std::endl
#ifdef MULTIPLE_DISASSEMBLERS_ENABLED
    #ifdef ENABLE_BINUTILS_DISASM
                                  << "Try running again with STF_DISASM=BINUTILS or updating to the latest version of Mavis"
    #endif
#else
                                  << "Rebuild stf_tools with binutils support or update to the latest version of Mavis"
#endif
                                  << std::endl;
                    }
                }

        };
    } //end namespace disassemblers
} //end namespace stf
