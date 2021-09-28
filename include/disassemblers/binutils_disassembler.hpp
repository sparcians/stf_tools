#pragma once

#define PACKAGE "trace_tools" // Needed to avoid compile error in dis-asm.h

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include "disassemble.h"
#pragma GCC diagnostic pop
}

#include "stf_exception.hpp"
#include "base_disassembler.hpp"
#include "util.hpp"

namespace stf {
    namespace disassemblers {
        /**
         * \class BinutilsDisassembler
         * \brief  Disassembler that uses binutils as a backend
         *
         */
        class BinutilsDisassembler : public BaseDisassembler {
            private:
                /**
                 * \class UnTabStream
                 * \brief Stream class that transforms tabs into spaces
                 */
                class UnTabStream {
                    private:
                        std::ostream& os_;
                        bool saw_tab_ = false;
                        size_t num_spaces_ = 0;
                        bool unknown_disasm_ = false;

                    public:
                        explicit UnTabStream(std::ostream& os) :
                            os_(os)
                        {
                        }

                        UnTabStream& operator<<(const char* rhs) {
                            /*
                              Disassembler will call this operator in this order:
                              1. opcode
                              2. '\t'
                              3. operand
                              4. ','
                              5. operand
                              6. repeat 4-5 for the rest of the operands
                             */
                            static constexpr size_t MNEMONIC_WIDTH = 12;

                            if(STF_EXPECT_TRUE(saw_tab_)) {
                                os_ << rhs;
                            }
                            else if(rhs[0] == '\t') {
                                saw_tab_ = true;
                                if(num_spaces_) {
                                    stf::format_utils::formatSpaces(os_, num_spaces_);
                                }
                                else {
                                    os_ << rhs;
                                }
                            }
                            else {
                                size_t idx = 0;
                                while(rhs[idx] != '\0') {
                                    os_ << rhs[idx];
                                    ++idx;
                                }
                                if(idx < MNEMONIC_WIDTH) {
                                    num_spaces_ = MNEMONIC_WIDTH - idx;
                                }
                            }

                            return *this;
                        }

                        void checkUnknownDisasm(const char* format) {
                            // The backend returns the hex value of the opcode if it can't successfully disassemble it
                            unknown_disasm_ |= (strcmp(format, "0x%llx") == 0);
                        }

                        bool hasUnknownDisasm() const {
                            return unknown_disasm_;
                        }
                };

                /**
                 * When disassembling to a string, the function that binutils will call.
                 */
                static int fprintf_(void * stream, const char * format, ... ) {
                    static constexpr size_t INITIAL_BUF_SIZE = 32;
                    static thread_local std::vector<char> buf(INITIAL_BUF_SIZE, '\0');

                    auto dis_str = static_cast<UnTabStream*>(stream);
                    stf_assert(dis_str, "Disassembly failed.");

                    int retval;
                    bool keep_going = false;

                    dis_str->checkUnknownDisasm(format);

                    do {
                        va_list ap;
                        va_start (ap, format);
                        retval = vsnprintf (buf.data(), buf.size(), format, ap);
                        if(static_cast<size_t>(retval) >= buf.size()) {
                            buf.resize(buf.size() << 1);
                            keep_going = true;
                        }
                        else {
                            keep_going = false;
                        }
                        va_end (ap);
                    }
                    while(keep_going);

                    *dis_str << buf.data();

                    return retval;
                };

                /**
                 * \brief Print the disassembly code of an opcode
                 * \param pc PC address of the instruction
                 * \param opcode Opcode of the instruction
                 * \param os The ostream to write the assembly to; default is std::cout
                 */
                void printDisassembly_(std::ostream& os,
                                       const uint64_t pc,
                                       const uint32_t opcode) const final {
                    UnTabStream dismStr(os);

                    dis_info_.stream = static_cast<void *>(&dismStr);

                    opcode_pc_ = pc;
                    copyU32_(opcode_mem_, opcode);
                    print_insn_riscv(opcode_pc_, &dis_info_);
                    unknown_disasm_ |= dismStr.hasUnknownDisasm();
                }

                /**
                 * Used by binutils to read an opcode
                 */
                inline int readMemory_(const bfd_vma memaddr,
                                       bfd_byte* const myaddr,
                                       const unsigned int length) {
                    const auto offset = static_cast<ssize_t>(memaddr - opcode_pc_);
                    const auto begin = std::next(opcode_mem_.begin(), offset);
                    const auto end = std::next(begin, length);
                    std::copy(begin, end, myaddr);
                    return 0;
                }

                /**
                 * Static wrapper for readMemory_ so that it can be used as a callback in binutils
                 */
                static inline int readMemoryStaticWrapper_(bfd_vma memaddr,
                                                           bfd_byte *myaddr,
                                                           unsigned int length,
                                                           struct disassemble_info *dinfo) {
                    return static_cast<BinutilsDisassembler*>(dinfo->application_data)->readMemory_(memaddr,
                                                                                                    myaddr,
                                                                                                    length);
                }

                /**
                 * Gets a specific byte from a given value
                 */
                template<typename T>
                static inline uint8_t getByte_(const T val, const int idx) {
                    static constexpr uint8_t BYTE_MASK = 0xFF;
                    return static_cast<uint8_t>((val >> byte_utils::toBits(idx)) & BYTE_MASK);
                }

                /**
                 * Copies a uint32_t to a 4-byte array
                 */
                static inline void copyU32_(std::array<uint8_t, 4>& array,
                                            const uint32_t val) {
                    array[0] = getByte_(val, 0);
                    array[1] = getByte_(val, 1);
                    array[2] = getByte_(val, 2);
                    array[3] = getByte_(val, 3);
                }

                /**
                 * Copies a uint16_t to a 4-byte array
                 */
                static inline void copyU16_(std::array<uint8_t, 4>& array,
                                            const uint16_t val) {
                    array[0] = getByte_(val, 0);
                    array[1] = getByte_(val, 1);
                    array[2] = 0;
                    array[3] = 0;
                }

                //! Disassmble information needed by the binutils disassembler
                mutable struct disassemble_info dis_info_;

                //! A 4 byte "memory" to hold an opcode for the binutils-required read memory function
                mutable std::array<uint8_t, 4> opcode_mem_;

                //! The PC of the opcode held in the 4 byte opcode memory
                mutable uint64_t opcode_pc_;

                //! Tracks whether we encountered an unknown instruction
                mutable bool unknown_disasm_ = false;

            public:
                /**
                 * \brief Construct a BinutilsDisassembler
                 */
                explicit BinutilsDisassembler(const ISA inst_set, const INST_IEM iem, const bool use_aliases) :
                    BaseDisassembler(inst_set, iem, use_aliases)
                {
                    init_disassemble_info (&dis_info_, 0, static_cast<fprintf_ftype>(fprintf_));
                    dis_info_.read_memory_func = readMemoryStaticWrapper_;
                    dis_info_.application_data = static_cast<void*>(this);
                    dis_info_.mach = bfd_mach_riscv64;
                    if (!use_aliases) {
                        dis_info_.disassembler_options = "no-aliases,numeric";
                    }
                    disassemble_init_for_target(&dis_info_);
                }

                ~BinutilsDisassembler() {
                    if(unknown_disasm_) {
                        std::cerr << "One or more unknown instructions were encountered. Try running again with STF_DISASM=MAVIS" << std::endl;
                    }
                }
        };
    } //end namespace disassemblers
} //end namespace stf
