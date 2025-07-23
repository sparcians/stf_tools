
// <Disassembler> -*- HPP -*-

/**
 * \brief  This file defines a Disassembler using binutil dis-asm functions
 *
 */

#pragma once

#include <memory>
#include "stf_env_var.hpp"
#include "stf_reader.hpp"

#ifdef ENABLE_BINUTILS_DISASM
    #include "disassemblers/binutils_disassembler.hpp"

    #ifndef MULTIPLE_DISASSEMBLERS_ENABLED
        #define MULTIPLE_DISASSEMBLERS_ENABLED
    #endif

    #define DEFAULT_DISASM_BACKEND "BINUTILS"
#else
    #define DEFAULT_DISASM_BACKEND "MAVIS"
#endif

#include "disassemblers/mavis_disassembler.hpp"

namespace stf {

#ifdef MULTIPLE_DISASSEMBLERS_ENABLED
    /**
     * \class Disassembler
     * If multiple disassembly backends are enabled at compile-time, the Disassembler class handles
     * choosing which backend to use based on the STF_DISASM environment variable
     */
    class Disassembler : public disassemblers::BaseDisassembler {
        private:
            std::unique_ptr<disassemblers::BaseDisassembler> dis_;

            /**
             * \brief Print the disassembly code of an opcode
             * \param pc PC address of the instruction
             * \param inst_set Instuction encoding mode
             * \param opcode Opcode of the instruction
             * \param os The ostream to write the assembly to
             */
            void printDisassembly_(std::ostream& os,
                                   const uint64_t pc,
                                   const uint32_t opcode) const final {
                dis_->printDisassembly(os, pc, opcode);
            }

        public:
            /**
             * Constructs a Disassembler
             */
            Disassembler(const std::string& elf,
                         const ISA inst_set,
                         const INST_IEM iem,
                         const std::string& isa_str,
                         const bool use_aliases) :
                BaseDisassembler(inst_set)
            {
                STFValidatedEnvVar disasm_var("STF_DISASM",
                                              {"MAVIS",
#ifdef ENABLE_BINUTILS_DISASM
                                               "BINUTILS",
#endif
                                              },
                                              DEFAULT_DISASM_BACKEND);
                if(const auto& disasm = disasm_var.get(); disasm == "MAVIS") {
                    dis_ = std::make_unique<disassemblers::MavisDisassembler>(elf,
                                                                              inst_set,
                                                                              iem,
                                                                              isa_str,
                                                                              use_aliases);
                }
#ifdef ENABLE_BINUTILS_DISASM
                else if(disasm == "BINUTILS") {
                    dis_ = std::make_unique<disassemblers::BinutilsDisassembler>(elf,
                                                                                 inst_set,
                                                                                 iem,
                                                                                 isa_str,
                                                                                 use_aliases);
                }
#endif
                else {
                    stf_throw("Invalid disassembler backend specified: " << disasm);
                }
            }

            Disassembler(const std::string& elf,
                         const STFReader& reader,
                         const bool use_aliases) :
                Disassembler(elf, reader.getISA(), reader.getInitialIEM(), reader.getISAExtendedInfo(), use_aliases)
            {
            }
    };
#else
    /**
     * \typedef Disassembler
     * If no other disassembly backends are enabled at compile-time, Disassembler becomes an alias
     * for disassemblers::MavisDisassembler
     */
    using Disassembler = disassemblers::MavisDisassembler;
#endif

} //end namespace stf
