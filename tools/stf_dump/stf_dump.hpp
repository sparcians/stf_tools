#pragma once

#include <cstdint>
#include <string>

/**
 * \struct STFDumpConfig
 * Holds configuration parsed from command line arguments
 */
struct STFDumpConfig {
    std::string trace_filename; /**< Trace to read */
    std::string symbol_filename; /**< File containing symbol table */
    bool concise_mode = false; /**< If true, only dumps PC info and disassembly */
    bool show_annotation = false; /**< If true, show symbol annotation */
    bool match_symbol_opcode = false; /**< If true, match opcodes against symbol opcodes */
    bool user_mode_only = false; /**< If true, only dump user-mode instructions */
    uint64_t start_inst = 0; /**< Start instruction */
    uint64_t end_inst = 0; /**< End instruction */
    bool use_aliases = false; /**< Use aliases when disassembling */
    bool show_pte = false; /**< Show PTE records */
};
