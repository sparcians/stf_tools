
// <Disassembler> -*- HPP -*-

/**
 * \brief  This file defines a Disassembler using binutil dis-asm functions
 *
 */

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

#include "format_utils.hpp"
#include "stf_enums.hpp"
#include "stf_exception.hpp"
#include "util.hpp"

namespace stf
{
    /**
     * \class Disassembler
     * \brief  A brain-dead disassembler
     *
     */
    class Disassembler {
        private:
            class BaseStream {
                public:
                    virtual ~BaseStream() = default;
                    virtual BaseStream& operator<<(const char* rhs) = 0;
            };

            template<typename OutputStreamType>
            class UnTabStream : public BaseStream {
                private:
                    OutputStreamType& os_;
                    bool saw_tab_ = false;
                    bool print_tab_ = false;
                    size_t num_spaces_ = 0;

                public:
                    explicit UnTabStream(OutputStreamType& os) :
                        os_(os)
                    {
                    }

                    // Disassembler will call this operator in this order:
                    // 1. opcode
                    // 2. '\t'
                    // 3. operand
                    // 4. ','
                    // 5. operand
                    // 6. repeat 4-5 for the rest of the operands
                    BaseStream& operator<<(const char* rhs) final {
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
        };

        public:
            /**
             * When disassmbling to a string, the function that binutils will call.
             */
            static int my_fprintf_ (void * stream, const char * format, ... ) {
                static constexpr size_t INITIAL_BUF_SIZE = 32;
                static thread_local std::vector<char> buf(INITIAL_BUF_SIZE, '\0');

                auto dis_str = static_cast<BaseStream*>(stream);
                stf_assert(dis_str, "Disassembly failed.");

                int retval;
                bool keep_going = false;
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
             * \brief Construct a Disassembler
             */
            explicit Disassembler (bool use_aliases) {
                //init_disassemble_info (&dis_info_, (void*)&output_, (fprintf_ftype)my_fprintf_);
                init_disassemble_info (&dis_info_, 0, static_cast<fprintf_ftype>(my_fprintf_));
                dis_info_.read_memory_func = my_read_memory_func_wrapper_;
                dis_info_.application_data = static_cast<void*>(this);
                dis_info_.mach = bfd_mach_riscv64;
                if (!use_aliases) {
                    dis_info_.disassembler_options = "no-aliases,numeric";
                }
                disassemble_init_for_target(&dis_info_);
            }

            /**
             * \brief Print the disassembly code of an opcode
             * \param pc PC address of the instruction
             * \param inst_set Instuction encoding mode
             * \param opcode Opcode of the instruction
             * \param os The ostream to write the assembly to; default is std::cout
             * \return number of opcode bytes used for disassembly
             */
            template<typename OutputStreamType = decltype(std::cout)>
            int printDisassembly (uint64_t pc,
                                  ISA inst_set,
                                  uint32_t opcode,
                                  OutputStreamType & os = std::cout) const {
                UnTabStream dismStr(os);

                dis_info_.stream = static_cast<void *>(&dismStr);
                int ret_val = 0;

                stf_assert(inst_set != ISA::RESERVED, "Invalid instruction set");

                opcode_pc_ = pc;
                if (inst_set == ISA::RISCV) {
                    uint32_to_mem_ (opcode_mem_, opcode);
                    ret_val = print_insn_riscv (opcode_pc_, &dis_info_);
                } else {
                    stf_throw("Unsupported printDisassembly");
                }

                return ret_val;
            }

            /**
             * \brief Print the opcode
             * \param inst_set Instuction encoding mode
             * \param opcode Opcode of the instruction
             * \param os The ostream to write the assembly to; default is std::cout
             */
            template<typename OutputStreamType = decltype(std::cout)>
            void printOpcode (ISA inst_set, uint32_t opcode,
                              OutputStreamType & os = std::cout) const {
                format_utils::formatOpcode(os, opcode);
                os << ' ';
            }

        private:

            int my_read_memory_func_ (bfd_vma memaddr,
                                      bfd_byte *myaddr,
                                      unsigned int length,
                                      struct disassemble_info *dinfo) {
                (void) dinfo;
                const auto offset = static_cast<ssize_t>(memaddr - opcode_pc_);
                const auto begin = std::next(opcode_mem_.begin(), offset);
                const auto end = std::next(begin, length);
                std::copy(begin, end, myaddr);
                return 0;
            }

            static int my_read_memory_func_wrapper_ (bfd_vma memaddr,
                                                     bfd_byte *myaddr,
                                                     unsigned int length,
                                                     struct disassemble_info *dinfo) {
                return static_cast<Disassembler *>(dinfo->application_data)->
                    my_read_memory_func_ (memaddr, myaddr, length, dinfo);
            }

            template<typename T>
            static inline uint8_t getByte(T val, const int idx) {
                static constexpr uint8_t BYTE_MASK = 0xFF;
                return static_cast<uint8_t>((val >> byte_utils::toBits(idx)) & BYTE_MASK);
            }

            static inline void uint32_to_mem_ (std::array<uint8_t, 4>& array, uint32_t val) {
                array[0] = getByte(val, 0);
                array[1] = getByte(val, 1);
                array[2] = getByte(val, 2);
                array[3] = getByte(val, 3);
            }

            static inline void uint16_to_mem_ (std::array<uint8_t, 4>& array, uint32_t val) {
                array[0] = getByte(val, 0);
                array[1] = getByte(val, 1);
                array[3] = array[2] = 0;
            }

        private:

            //! Disassmble information needed by the binutils disassembler
            mutable struct disassemble_info dis_info_;

            //! A 4 byte "memory" to hold an opcode for the binutils-required read memory function
            mutable std::array<uint8_t, 4> opcode_mem_;

            //! The PC of the opcode held in the 4 byte opcode memory
            mutable uint64_t opcode_pc_;
    };

} //end namespace stf
