#include "binutils_c_wrapper.h"
#include "libbfd.h"

void init_section_hash_table(bfd* abfd)
{
    bfd_hash_table_init_n(&abfd->section_htab, bfd_hash_newfunc, sizeof(struct section_hash_entry), 1);
}
