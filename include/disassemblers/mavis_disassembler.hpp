#pragma once

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

        };
    } //end namespace disassemblers
} //end namespace stf
