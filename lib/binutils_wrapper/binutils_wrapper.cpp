extern "C" {
#include "binutils_c_wrapper.h"
}

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

#if BFD_VERSION >= 239000000
    #define ENABLE_STYLED_FPRINTF_WRAPPER
#endif

class RISCVXExtensionMissingVersion : public std::exception
{
    private:
        std::string extension_;

    public:
        explicit RISCVXExtensionMissingVersion(const char* extension) :
            extension_(extension)
        {
        }

        const std::string& getExtension() const { return extension_; }
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

// cppcheck-suppress unusedFunction
void opcodes_error_handler(const char* fmt, ...) {
    va_list args;

    // This error message is defined and checked in lib/binutils_wrapper/CMakeLists.txt
    // If it changes, building this library should fail
    if(strcmp(fmt, BINUTILS_XEXT_VERSION_ERROR_MESSAGE) == 0)
    {
        va_start(args, fmt);
        char* extension = va_arg(args, char*);
        va_end(args);
        const RISCVXExtensionMissingVersion ex(extension);
        // The calling function makes a copy of the extension string and would ordinarily free it after
        // the error handler function returns
        // Since we're about to throw an exception, we'll take care of freeing it ourselves
        free(static_cast<void*>(extension));
        throw ex;
    }
    else
    {
        const char* formatted_str;
        va_start(args, fmt);
        varListFormatter(&formatted_str, fmt, args);
        va_end(args);

        stf_throw("Error encountered in BFD library: " + std::string(formatted_str));
    }
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
                // cppcheck-suppress cstyleCast
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

#ifdef ENABLE_STYLED_FPRINTF_WRAPPER
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
#endif

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
            // cppcheck-suppress unusedFunction
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

            void freePrivateData_() const {
                if(dis_info_.private_data) {
                    disassemble_free_riscv(&dis_info_);
                    free(dis_info_.private_data);
                    dis_info_.private_data = nullptr;
                }
            }

            template<typename SetupFunc>
            void binutilsSetupRetryWrapper_(SetupFunc&& setup_func) const {
                bool success = false;

                do {
                    try {
                        setup_func();
                        success = true;
                    }
                    catch(const RISCVXExtensionMissingVersion& ex) {
                        dummy_bfd_.addDefaultVersionToExtension(ex.getExtension());
                        freePrivateData_();
                    }
                }
                while(!success);
            }

            // Populates *just enough* libbfd fields for the disassembler to successfully grab
            // the ISA string as if this was a valid ELF
            class ISAStringBFD
            {
                private:
                    inline static const char* OBJ_ATTRS_SECTION_ = ".RISCV.attributes";

                    std::string isa_string_;
                    elf_backend_data backend_data_;
                    bfd_target target_;
                    elf_obj_tdata elf_tdata_;
                    bfd bfd_;
                    asection section_;

                    static inline std::string initISAString_(const std::string& elf,
                                                             const std::string& default_isa) {
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

                        return isa_str;
                    }

                    void updateISAStringPointer_() {
                        elf_tdata_.known_obj_attributes[OBJ_ATTR_PROC][Tag_RISCV_arch].s = isa_string_.data();
                    }

                public:
                    ISAStringBFD(const std::string& elf, const std::string& default_isa) :
                        isa_string_(initISAString_(elf, default_isa))
                    {
                        backend_data_.obj_attrs_section = OBJ_ATTRS_SECTION_;

                        target_.flavour = bfd_target_elf_flavour;
                        target_.backend_data = static_cast<void*>(&backend_data_);

                        auto* riscv_attrs = elf_tdata_.known_obj_attributes[OBJ_ATTR_PROC];
                        riscv_attrs[Tag_RISCV_priv_spec].i = 0;
                        riscv_attrs[Tag_RISCV_priv_spec_minor].i = 0;
                        riscv_attrs[Tag_RISCV_priv_spec_revision].i = 0;
                        updateISAStringPointer_();

                        init_section_hash_table(&bfd_);
                        bfd_hash_lookup(&bfd_.section_htab, OBJ_ATTRS_SECTION_, true, false);

                        bfd_.xvec = &target_;
                        bfd_.tdata.elf_obj_data = &elf_tdata_;

                        section_.owner = &bfd_;
                        section_.flags |= SEC_CODE;
                    }

                    ~ISAStringBFD() {
                        bfd_hash_table_free(&bfd_.section_htab);
                    }

                    void addDefaultVersionToExtension(const std::string& extension) {
                        size_t pos = 0;
                        size_t ext_end = 0;

                        do {
                            // Find the next occurrence of the extension string
                            pos = isa_string_.find(extension, pos);

                            stf_assert(pos > 0, "Extension " << extension << " cannot occur at the beginning of an ISA string");

                            stf_assert(
                                pos != std::string::npos,
                                "Extension " << extension <<
                                " is missing a version but is not present in the ISA string: " << isa_string_
                            );

                            // Get one-past the end of the extension substring
                            ext_end = pos + extension.size();

                            stf_assert(ext_end <= isa_string_.size());

                            // If the extension substring is not the beginning of the string, it should have an _ before it
                            // If it's not at the end of the string, it should have an _ after it
                            // Otherwise, we found a longer extension name that contains the extension we're actually trying to find
                            if((isa_string_[pos - 1] != '_') || (ext_end != isa_string_.size() && isa_string_[ext_end] != '_')) {
                                // Jump ahead to the end of this match so we can search again
                                pos = ext_end;
                            }
                            else {
                                break;
                            }
                        }
                        while(true);

                        // Add a default version number to the end of the extension
                        isa_string_.insert(ext_end, "1p0");
                        updateISAStringPointer_();
                    }

                    asection* getSectionPtr() { return &section_; }

                    bfd* getBFDPtr() { return &bfd_; }
            };

            mutable ISAStringBFD dummy_bfd_;

            //! Disassemble information needed by the binutils disassembler
            mutable struct disassemble_info dis_info_;

            //! A 4 byte "memory" to hold an opcode for the binutils-required read memory function
            mutable std::array<uint8_t, 4> opcode_mem_{};

            //! The PC of the opcode held in the 4 byte opcode memory
            mutable uint64_t opcode_pc_ = 0;

            mutable UnTabStream stream_;

        public:
            DisassemblerInternals(const std::string& elf,
                                  const unsigned long bfd_mach,
                                  const std::string& default_isa,
                                  const bool use_aliases) :
                dummy_bfd_(elf, default_isa)
            {

                init_disassemble_info (
                    &dis_info_,
                    0,
                    static_cast<fprintf_ftype>(fprintf_wrapper_)
#ifdef ENABLE_STYLED_FPRINTF_WRAPPER
                    , static_cast<fprintf_styled_ftype>(fprintf_styled_wrapper_)
#endif
                );

                dis_info_.read_memory_func = readMemoryWrapper_;
                dis_info_.application_data = static_cast<void*>(this);
                dis_info_.arch = bfd_arch_riscv;
                dis_info_.mach = bfd_mach;
                dis_info_.section = dummy_bfd_.getSectionPtr();
                dis_info_.stream = static_cast<void *>(&stream_);
                if (!use_aliases) {
                    dis_info_.disassembler_options = "no-aliases,numeric";
                }

                disassemble_init_for_target(&dis_info_);

                // For binutils versions >= 2.45, print_insn_riscv automatically
                // sets up the disassembler
                // For older versions, we have to call riscv_get_disassembler
#if BFD_VERSION < 245000000
                binutilsSetupRetryWrapper_([this]() {
                    riscv_get_disassembler(dummy_bfd_.getBFDPtr());
                });
#endif
            }

            ~DisassemblerInternals() {
                freePrivateData_();
            }

            bool disassemble(std::ostream& os,
                             const uint64_t pc,
                             const uint32_t opcode) const {
                stream_.reset(os);
                opcode_pc_ = pc;
                copyU32_(opcode_mem_, opcode);
#if BFD_VERSION >= 245000000
                binutilsSetupRetryWrapper_([this]() {
                    print_insn_riscv(opcode_pc_, &dis_info_);
                });
#else
                print_insn_riscv(opcode_pc_, &dis_info_);
#endif

                return stream_.hasUnknownDisasm();
            }
    };
}

namespace stf {
    namespace disassemblers {
        unsigned long BinutilsDisassembler::getBfdMach_(const INST_IEM iem)
        {
            switch(iem)
            {
                case INST_IEM::STF_INST_IEM_RV32:
                    return bfd_mach_riscv32;
                case INST_IEM::STF_INST_IEM_RV64:
                    return bfd_mach_riscv64;
                case INST_IEM::STF_INST_IEM_INVALID:
                case INST_IEM::STF_INST_IEM_RESERVED:
                    break;
            }

            stf_throw("Invalid INST_IEM value: " << iem);
        }

        BinutilsDisassembler::BinutilsDisassembler(const std::string& elf,
                                                   const ISA inst_set,
                                                   const INST_IEM iem,
                                                   const std::string& isa_str,
                                                   const bool use_aliases) :
            BaseDisassembler(inst_set),
            dis_(std::make_unique<binutils_wrapper::DisassemblerInternals>(elf, getBfdMach_(iem), isa_str, use_aliases))
        {
        }

        BinutilsDisassembler::BinutilsDisassembler(const std::string& elf, const STFReader& reader, const bool use_aliases) :
            BinutilsDisassembler(elf, reader.getISA(), reader.getInitialIEM(), reader.getISAExtendedInfo(), use_aliases)
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
