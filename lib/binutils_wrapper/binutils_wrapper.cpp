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
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wpedantic"
#include "disassemble.h"
#include "elf-bfd.h"
#include "elf/riscv.h"
#pragma GCC diagnostic pop
}

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

namespace binutils_wrapper {
    class Bfd {
        private:
            static bool bfd_initialized_;
            static size_t bfd_count_;

            static inline void init_() {
                if(!bfd_initialized_) {
                    bfd_init();
                    bfd_initialized_ = true;
                }
                ++bfd_count_;
            }

            static inline void close_(bfd* abfd) {
                stf_assert(bfd_initialized_, "Attempted to close a BFD handle without initializing library");

                --bfd_count_;
                if(bfd_count_ > 0) {
                    if(abfd) {
                        bfd_close(abfd);
                    }
                }
                else {
                    if(abfd) {
                        bfd_close_all_done(abfd);
                    }
                    bfd_initialized_ = false;
                }
            }

            bfd* bfd_ = nullptr;

        public:
            class ElfNotFoundException : public std::exception {
            };

            Bfd() = default;

            explicit Bfd(const std::string& elf) :
                Bfd()
            {
                open(elf);
            }

            ~Bfd() {
                close_(bfd_);
            }

            inline void open(const std::string& elf) {
                init_();
                bfd_ = bfd_openr(elf.c_str(), nullptr);
                if(STF_EXPECT_FALSE(!bfd_)) {
                    throw ElfNotFoundException();
                }
                stf_assert(bfd_check_format(bfd_, bfd_object), "Specified ELF is not an object file");
            }

            inline const auto* getSection(const char* name) const {
                return bfd_get_section_by_name(bfd_, name);
            }

            inline const obj_attribute* getAttributes() const {
                if(getSection(get_elf_backend_data(bfd_)->obj_attrs_section)) {
                    return elf_known_obj_attributes_proc(bfd_);
                }

                return nullptr;
            }
    };

    bool Bfd::bfd_initialized_ = false;
    size_t Bfd::bfd_count_ = 0;

    class DisassemblerInternals {
        private:
            /**
             * \class UnTabStream
             * \brief Stream class that transforms tabs into spaces
             */
            class UnTabStream {
                private:
                    std::ostream* os_ = nullptr;
                    bool saw_tab_ = false;
                    size_t num_spaces_ = 0;
                    bool unknown_disasm_ = false;

                public:
                    UnTabStream() = default;

                    explicit UnTabStream(std::ostream& os) :
                        os_(&os)
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
                        auto& os = *os_;

                        if(STF_EXPECT_TRUE(saw_tab_)) {
                            os << rhs;
                        }
                        else if(rhs[0] == '\t') {
                            saw_tab_ = true;
                            if(num_spaces_) {
                                stf::format_utils::formatSpaces(os, num_spaces_);
                            }
                            else {
                                os << rhs;
                            }
                        }
                        else {
                            size_t idx = 0;
                            while(rhs[idx] != '\0') {
                                os << rhs[idx];
                                ++idx;
                            }
                            if(idx < MNEMONIC_WIDTH) {
                                num_spaces_ = MNEMONIC_WIDTH - idx;
                            }
                        }

                        return *this;
                    }

                    void reset(std::ostream& os) {
                        os_ = &os;
                        saw_tab_ = false;
                        num_spaces_ = 0;
                        unknown_disasm_ = false;
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
             * fprintf handler that takes a va_list of arguments
             */
            static int vfprintf_wrapper_(void* stream, const char* format, va_list args) {
                auto dis_str = static_cast<UnTabStream*>(stream);
                stf_assert(dis_str, "Disassembly failed.");

                dis_str->checkUnknownDisasm(format);

                const char* formatted_str = nullptr;
                const auto retval = varListFormatter(&formatted_str, format, args);

                *dis_str << formatted_str;

                return retval;
            }

            /**
             * When disassembling to a string, the function that binutils will call.
             */
            static int fprintf_wrapper_(void* stream, const char* format, ...) {
                va_list args;
                va_start(args, format);
                const auto retval = vfprintf_wrapper_(stream, format, args);
                va_end(args);

                return retval;
            }

            /**
             * Binutils 2.39 added a styled fprintf option for syntax highlighting.
             * Right now this is a stub that will call the existing (unformatted) fprintf handler.
             */
            static int fprintf_styled_wrapper_(void* stream,
                                               enum disassembler_style,
                                               const char* format,
                                               ...) {
                va_list args;
                va_start(args, format);
                const auto retval = vfprintf_wrapper_(stream, format, args);
                va_end(args);

                return retval;
            }

            /**
             * Gets a specific byte from a given value
             */
            template<typename T>
            static inline uint8_t getByte_(const T val, const int idx) {
                static constexpr uint8_t BYTE_MASK = 0xFF;
                return static_cast<uint8_t>((val >> stf::byte_utils::toBits(idx)) & BYTE_MASK);
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

            static inline disassembler_ftype initDisasmFunc_(const std::string& elf,
                                                             const char* default_isa) {
                std::string isa_str;
                if(!elf.empty()) {
                    try {
                        Bfd bfd(elf);
                        if(auto* attr = bfd.getAttributes()) {
                            isa_str = attr[Tag_RISCV_arch].s;
                        }
                    }
                    catch(const Bfd::ElfNotFoundException&) {
                    }
                }

                if(isa_str.empty()) {
                    isa_str = stf::STFEnvVar("STF_DISASM_ISA", default_isa).get();
                }

                return riscv_get_disassembler_arch(nullptr, isa_str.c_str());
            }

            //! Disassembler function pointer
            const disassembler_ftype disasm_func_;

            //! Disassemble information needed by the binutils disassembler
            mutable struct disassemble_info dis_info_;

            //! A 4 byte "memory" to hold an opcode for the binutils-required read memory function
            mutable std::array<uint8_t, 4> opcode_mem_;

            //! The PC of the opcode held in the 4 byte opcode memory
            mutable uint64_t opcode_pc_ = 0;

            mutable UnTabStream dismStr_;

        public:
            DisassemblerInternals(const std::string& elf,
                                  const char* default_isa,
                                  const bool use_aliases) :
                disasm_func_(initDisasmFunc_(elf, default_isa))
            {
                init_disassemble_info (&dis_info_,
                                       0,
                                       static_cast<fprintf_ftype>(fprintf_wrapper_),
                                       static_cast<fprintf_styled_ftype>(fprintf_styled_wrapper_));
                dis_info_.read_memory_func = readMemoryWrapper_;
                dis_info_.application_data = static_cast<void*>(this);
                dis_info_.mach = bfd_mach_riscv64;
                dis_info_.stream = static_cast<void *>(&dismStr_);
                if (!use_aliases) {
                    dis_info_.disassembler_options = "no-aliases,numeric";
                }
                disassemble_init_for_target(&dis_info_);
            }

            bool disassemble(std::ostream& os,
                             const uint64_t pc,
                             const uint32_t opcode) const {
                dismStr_.reset(os);
                opcode_pc_ = pc;
                copyU32_(opcode_mem_, opcode);
                print_insn_riscv(opcode_pc_, &dis_info_);
                return dismStr_.hasUnknownDisasm();
            }
    };
}

namespace stf {
    namespace disassemblers {
        BinutilsDisassembler::BinutilsDisassembler(const std::string& elf,
                                                   const ISA inst_set,
                                                   const INST_IEM iem,
                                                   const bool use_aliases) :
            BaseDisassembler(inst_set, iem, use_aliases),
            dis_(std::make_unique<binutils_wrapper::DisassemblerInternals>(elf, DEFAULT_DISASM_ISA_, use_aliases))
        {
        }

        BinutilsDisassembler::~BinutilsDisassembler() {
            if(unknown_disasm_) {
                std::cerr << "One or more unknown instructions were encountered. "
                             "Try running again with a different ISA string specified in STF_DISASM_ISA. "
                             "Alternatively, you can try STF_DISASM=MAVIS" << std::endl;
            }
        }

        void BinutilsDisassembler::printDisassembly_(std::ostream& os,
                                                     const uint64_t pc,
                                                     const uint32_t opcode) const {
            unknown_disasm_ |= dis_->disassemble(os, pc, opcode);
        }

    }
}
