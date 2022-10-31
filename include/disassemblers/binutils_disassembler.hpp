#pragma once

#include <iostream>
#include <memory>
#include <string_view>

#include "base_disassembler.hpp"
#include "binutils_wrapper.hpp"

namespace stf {
    namespace disassemblers {
        /**
         * \class BinutilsDisassembler
         * \brief  Disassembler that uses binutils as a backend
         *
         */
        class BinutilsDisassembler : public BaseDisassembler {
            private:
                std::unique_ptr<binutils_wrapper::DisassemblerInternals> dis_;

                //! Tracks whether we encountered an unknown instruction
                mutable bool unknown_disasm_ = false;

                /**
                 * \brief Print the disassembly code of an opcode
                 * \param pc PC address of the instruction
                 * \param opcode Opcode of the instruction
                 * \param os The ostream to write the assembly to; default is std::cout
                 */
                void printDisassembly_(std::ostream& os,
                                       const uint64_t pc,
                                       const uint32_t opcode) const final;

                inline static const char* DEFAULT_DISASM_ISA_ = "rv64gcv_zba_zbb_zbs";

            public:
                /**
                 * \brief Construct a BinutilsDisassembler
                 */
                explicit BinutilsDisassembler(const ISA inst_set, const INST_IEM iem, const bool use_aliases);

                ~BinutilsDisassembler();
        };
    } //end namespace disassemblers
} //end namespace stf
