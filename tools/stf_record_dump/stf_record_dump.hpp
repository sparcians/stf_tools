#pragma once

#include <cstdint>
#include <string>

/**
 * \struct STFDumpConfig
 * Holds configuration parsed from command line arguments
 */
struct STFRecordDumpConfig {
    std::string trace_filename; /**< Trace to read */
    std::string symbol_filename; /**< File containing symbol table */
    bool show_annotation = false; /**< If true, show symbol annotation */
    uint64_t start_inst = 0; /**< Start instruction */
    uint64_t end_inst = 0; /**< End instruction */
    uint64_t start_record = 0; /**< Start instruction */
    uint64_t end_record = 0; /**< End instruction */
    bool use_aliases = false; /**< Use aliases when disassembling */
};
