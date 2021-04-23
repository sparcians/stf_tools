#pragma once

#include <cstdint>
#include <ostream>

#include "format_utils.hpp"
#include "stf_enums.hpp"

#include "file_utils.hpp"

namespace stf {
    namespace disassemblers {
        class BaseDisassembler {
            protected:
                /**
                 * \brief Print the disassembly code of an opcode
                 * \param pc PC address of the instruction
                 * \param opcode Opcode of the instruction
                 * \param os The ostream to write the assembly to
                 */
                virtual void printDisassembly_(std::ostream& os,
                                               const uint64_t pc,
                                               const uint32_t opcode) const = 0;
            public:
                BaseDisassembler(const ISA inst_set, const bool use_aliases) {
                    stf_assert(inst_set == ISA::RISCV, "Invalid instruction set: " << inst_set);
                }

                virtual ~BaseDisassembler() = default;

                /**
                 * \brief Print the disassembly code of an opcode
                 * \param pc PC address of the instruction
                 * \param opcode Opcode of the instruction
                 * \param os The ostream to write the assembly to
                 */
                inline void printDisassembly(std::ostream& os,
                                             const uint64_t pc,
                                             const uint32_t opcode) const {
                    printDisassembly_(os, pc, opcode);
                }

                /**
                 * \brief Print the disassembly code of an opcode
                 * \param pc PC address of the instruction
                 * \param opcode Opcode of the instruction
                 * \param os The OutputFileStream to write the assembly to
                 */
                inline void printDisassembly(OutputFileStream& os,
                                             const uint64_t pc,
                                             const uint32_t opcode) const {
                    printDisassembly(os.getStream(), pc, opcode);
                }

                /**
                 * \brief Print the opcode
                 * \param opcode Opcode of the instruction
                 * \param os The ostream to write the assembly to
                 */
                inline void printOpcode(std::ostream& os,
                                        const uint32_t opcode) const {
                    format_utils::formatOpcode(os, opcode);
                    os << ' ';
                }

                /**
                 * \brief Print the opcode
                 * \param opcode Opcode of the instruction
                 * \param os The OutputFileStream to write the assembly to
                 */
                inline void printOpcode(OutputFileStream& os,
                                        const uint32_t opcode) const {
                    printOpcode(os.getStream(), opcode);
                }
        };
    } //end namespace disassemblers
} //end namespace stf
