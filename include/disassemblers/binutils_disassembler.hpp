#pragma once

#include <iostream>
#include <memory>
#include <string_view>

#include "base_disassembler.hpp"
#include "binutils_wrapper.hpp"
#include "stf_enums.hpp"

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

#define DEFAULT_RISCV_ISA_EXTENSIONS "imafdcvh_zicbom_zicbop_zicboz_zicsr_zicond_zifencei_zihintpause_zfhmin_zba_zbb_zbs_zihintntl_zvbb_zvbc_zvkg_zvkned_zvknhb_zvksed_zvksh_zvkt_smaia_smstateen_ss1p12_ssaia_sscofpmf_ssstateen_sv48_svinval_svnapot_svpbmt"
#define GEN_DEFAULT_RISCV_ISA_STRING(bits) "rv" #bits DEFAULT_RISCV_ISA_EXTENSIONS

                inline static constexpr char RV32_DEFAULT_DISASM_ISA_[] = GEN_DEFAULT_RISCV_ISA_STRING(32);
                inline static constexpr char RV64_DEFAULT_DISASM_ISA_[] = GEN_DEFAULT_RISCV_ISA_STRING(64);

#undef DEFAULT_RISCV_ISA_EXTENSIONS
#undef GEN_DEFAULT_RISCV_ISA_STRING

                static unsigned long getBfdMach_(const INST_IEM iem);

                inline static const char* getDefaultDisasmISA_(const INST_IEM iem)
                {
                    switch(iem)
                    {
                        case INST_IEM::STF_INST_IEM_RV32:
                            return RV32_DEFAULT_DISASM_ISA_;
                        case INST_IEM::STF_INST_IEM_RV64:
                            return RV64_DEFAULT_DISASM_ISA_;
                        case INST_IEM::STF_INST_IEM_INVALID:
                        case INST_IEM::STF_INST_IEM_RESERVED:
                            break;
                    }

                    stf_throw("Invalid INST_IEM value: " << iem);
                }

            public:
                /**
                 * \brief Construct a BinutilsDisassembler
                 */
                explicit BinutilsDisassembler(const std::string& elf, const ISA inst_set, const INST_IEM iem, const bool use_aliases);

                ~BinutilsDisassembler();
        };
    } //end namespace disassemblers
} //end namespace stf
