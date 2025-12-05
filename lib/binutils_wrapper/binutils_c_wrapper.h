#ifndef __BINUTILS_C_WRAPPER_H__
#define __BINUTILS_C_WRAPPER_H__

#include "config.h"
#include "disassemble.h"
#include "elf-bfd.h"
#include "elf/riscv.h"
#include "bfdver.h"
#include "elf-bfd.h"

void init_section_hash_table(bfd* abfd);

#endif
