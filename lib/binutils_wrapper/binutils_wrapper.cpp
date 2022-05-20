#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "binutils_wrapper.hpp"
#include "disassemblers/binutils_disassembler.hpp"
#include "stf_env_var.hpp"
#include "stf_exception.hpp"
#include "format_utils.hpp"
#include "util.hpp"

#define PACKAGE "trace_tools" // Needed to avoid compile error in dis-asm.h

#define OVERRIDE_OPCODES_ERROR_HANDLER 1

extern "C" {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include "disassemble.h"
#pragma GCC diagnostic pop
}

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

static inline int varListFormatter(const char** output_str, const char* format, va_list args) {
    static constexpr size_t INITIAL_BUF_SIZE = 32;
    static thread_local std::vector<char> buf(INITIAL_BUF_SIZE, '\0');

    int retval;
    bool keep_going = false;

    do {
        va_list args_copy;
        va_copy(args_copy, args);
        retval = vsnprintf (buf.data(), buf.size(), format, args_copy);
        if(static_cast<size_t>(retval) >= buf.size()) {
            buf.resize(buf.size() << 1);
            keep_going = true;
        }
        else {
            keep_going = false;
        }
        va_end(args_copy);
    }
    while(keep_going);

    *output_str = buf.data();
    return retval;
}

void opcodes_error_handler(const char* fmt, ...) {
    const char* formatted_str;
    va_list args;
    va_start(args, fmt);
    varListFormatter(&formatted_str, fmt, args);
    va_end(args);

    stf_throw("Error encountered in BFD library: " + std::string(formatted_str));
}

/**
 * When disassembling to a string, the function that binutils will call.
 */
static int fprintf_wrapper(void * stream, const char * format, ... ) {
    auto dis_str = static_cast<UnTabStream*>(stream);
    stf_assert(dis_str, "Disassembly failed.");

    dis_str->checkUnknownDisasm(format);

    const char* formatted_str = nullptr;
    va_list args;
    va_start(args, format);
    const auto retval = varListFormatter(&formatted_str, format, args);
    va_end(args);

    *dis_str << formatted_str;

    return retval;
}

static inline disassembler_ftype initDisasmFunc(const std::string& riscv_isa_str) {
    return riscv_get_disassembler_arch(nullptr, riscv_isa_str.c_str());
}

/**
 * Gets a specific byte from a given value
 */
template<typename T>
static inline uint8_t getByte(const T val, const int idx) {
    static constexpr uint8_t BYTE_MASK = 0xFF;
    return static_cast<uint8_t>((val >> stf::byte_utils::toBits(idx)) & BYTE_MASK);
}

/**
 * Copies a uint32_t to a 4-byte array
 */
static inline void copyU32(std::array<uint8_t, 4>& array,
                            const uint32_t val) {
    array[0] = getByte(val, 0);
    array[1] = getByte(val, 1);
    array[2] = getByte(val, 2);
    array[3] = getByte(val, 3);
}

/**
 * Copies a uint16_t to a 4-byte array
 */
static inline void copyU16(std::array<uint8_t, 4>& array,
                            const uint16_t val) {
    array[0] = getByte(val, 0);
    array[1] = getByte(val, 1);
    array[2] = 0;
    array[3] = 0;
}

namespace binutils_wrapper {
    class DisassemblerInternals {
        private:
            //! Disassembler function pointer
            const disassembler_ftype disasm_func_;

            //! Disassemble information needed by the binutils disassembler
            mutable struct disassemble_info dis_info_;

            //! A 4 byte "memory" to hold an opcode for the binutils-required read memory function
            mutable std::array<uint8_t, 4> opcode_mem_;

            //! The PC of the opcode held in the 4 byte opcode memory
            mutable uint64_t opcode_pc_;

            /**
             * Static wrapper for readMemory_ so that it can be used as a callback in binutils
             */
            static int readMemoryWrapper_(bfd_vma memaddr,
                                          bfd_byte *myaddr,
                                          unsigned int length,
                                          struct disassemble_info *dinfo) {
                return static_cast<binutils_wrapper::DisassemblerInternals*>(dinfo->application_data)->readMemory_(memaddr,
                                                                                                                         myaddr,
                                                                                                                         length);
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

        public:
            DisassemblerInternals(const std::string& riscv_isa_str, const bool use_aliases) :
                disasm_func_(initDisasmFunc(riscv_isa_str))
            {
                init_disassemble_info (&dis_info_, 0, static_cast<fprintf_ftype>(fprintf_wrapper));
                dis_info_.read_memory_func = readMemoryWrapper_;
                dis_info_.application_data = static_cast<void*>(this);
                dis_info_.mach = bfd_mach_riscv64;
                if (!use_aliases) {
                    dis_info_.disassembler_options = "no-aliases,numeric";
                }
                disassemble_init_for_target(&dis_info_);
            }

            bool disassemble(std::ostream& os,
                             const uint64_t pc,
                             const uint32_t opcode) const {
                UnTabStream dismStr(os);

                dis_info_.stream = static_cast<void *>(&dismStr);

                opcode_pc_ = pc;
                copyU32(opcode_mem_, opcode);
                print_insn_riscv(opcode_pc_, &dis_info_);
                return dismStr.hasUnknownDisasm();
            }
    };
}

namespace stf {
    namespace disassemblers {
        BinutilsDisassembler::BinutilsDisassembler(const ISA inst_set, const INST_IEM iem, const bool use_aliases) :
            BaseDisassembler(inst_set, iem, use_aliases),
            dis_(std::make_unique<binutils_wrapper::DisassemblerInternals>(STFEnvVar("STF_DISASM_ISA", DEFAULT_DISASM_ISA_).get(), use_aliases))
        {
        }

        BinutilsDisassembler::~BinutilsDisassembler() {
            if(unknown_disasm_) {
                std::cerr << "One or more unknown instructions were encountered. Try running again with a different ISA string specified in STF_DISASM_ISA. Alternatively, you can try STF_DISASM=MAVIS" << std::endl;
            }
        }

        void BinutilsDisassembler::printDisassembly_(std::ostream& os,
                                                     const uint64_t pc,
                                                     const uint32_t opcode) const {
            unknown_disasm_ |= dis_->disassemble(os, pc, opcode);
        }

    }
}
