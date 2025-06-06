#pragma once

#include "stf_enums.hpp"

#define DEFAULT_RISCV_ISA_EXTENSIONS "imafdcvh_zicbom_zicbop_zicboz_zicsr_zicond_zifencei_zihintpause_zfhmin_zba_zbb_zbs_zihintntl_zvbb_zvbc_zvkg_zvkned_zvknhb_zvksed_zvksh_zvkt_smaia_smstateen_ss1p12_ssaia_sscofpmf_ssstateen_sv48_svinval_svnapot_svpbmt"
#define GEN_DEFAULT_RISCV_ISA_STRING(bits) "rv" #bits DEFAULT_RISCV_ISA_EXTENSIONS

#ifndef DEFAULT_RV32_ISA_STRING
#define DEFAULT_RV32_ISA_STRING GEN_DEFAULT_RISCV_ISA_STRING(32)
#endif

#ifndef DEFAULT_RV64_ISA_STRING
#define DEFAULT_RV64_ISA_STRING GEN_DEFAULT_RISCV_ISA_STRING(64)
#endif

namespace stf::isa_defs {
    namespace riscv {
        inline static constexpr char RV32_DEFAULT_DISASM_ISA[] = DEFAULT_RV32_ISA_STRING;
        inline static constexpr char RV64_DEFAULT_DISASM_ISA[] = DEFAULT_RV64_ISA_STRING;
    }

    static inline const char* getDefaultISAString(const stf::INST_IEM iem) {
        switch(iem) {
            case stf::INST_IEM::STF_INST_IEM_RV32:
                return riscv::RV32_DEFAULT_DISASM_ISA;
            case stf::INST_IEM::STF_INST_IEM_RV64:
                return riscv::RV64_DEFAULT_DISASM_ISA;
            case stf::INST_IEM::STF_INST_IEM_INVALID:
            case stf::INST_IEM::STF_INST_IEM_RESERVED:
                break;
        }

        stf_throw("Invalid IEM: " << iem);
    }
}

#undef DEFAULT_RISCV_ISA_EXTENSIONS
#undef GEN_DEFAULT_RISCV_ISA_STRING
