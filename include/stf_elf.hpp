#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

#include <boost/core/demangle.hpp>

#include "filesystem.hpp"
#include "stf_bin.hpp"

#include "elfio/elfio.hpp"

#include "dwarf.h"
#include "libdwarf.h"

#define DWARF_ERROR(prefix) prefix << ": " << dwarf_errmsg(err)
#define DWARF_ASSERT_OK(err_prefix) stf_assert(res == DW_DLV_OK, DWARF_ERROR(err_prefix))
#define DWARF_ASSERT_NOERR(err_prefix) stf_assert(res != DW_DLV_ERROR, DWARF_ERROR(err_prefix))

class STFElf : public STFBinary {
    private:
        static constexpr int DWARF_TRUE  = 1;
        static constexpr int DWARF_FALSE = 0;

        using Range = std::pair<uint64_t, uint64_t>;
        using RangeVector = std::vector<Range>;

        class RangeKey {
            private:
                uint64_t start_pc_;
                uint64_t end_pc_;

            public:
                explicit RangeKey(const Range& range) :
                    RangeKey(range.first, range.second)
                {
                }

                RangeKey(const uint64_t start_pc, const uint64_t end_pc) :
                    start_pc_(start_pc),
                    end_pc_(end_pc)
                {
                    stf_assert(start_pc_ < end_pc_, "End PC must be larger than start PC");
                }

                inline uint64_t startAddress() const {
                    return start_pc_;
                }

                inline uint64_t endAddress() const {
                    return end_pc_;
                }

                inline uint64_t range() const {
                    return end_pc_ - start_pc_;
                }

                inline bool startsBefore(const RangeKey& rhs) const {
                    return start_pc_ < rhs.start_pc_;
                }

                inline bool startsAfter(const RangeKey& rhs) const {
                    return start_pc_ > rhs.start_pc_;
                }

                inline bool operator<(const RangeKey& rhs) const {
                    return startsBefore(rhs) || ((start_pc_ == rhs.start_pc_) && (range() < rhs.range()));
                }

                inline bool operator>(const RangeKey& rhs) const {
                    return startsAfter(rhs) || ((start_pc_ == rhs.start_pc_) && (range() > rhs.range()));
                }

                inline bool operator==(const RangeKey& rhs) const {
                    return (start_pc_ == rhs.start_pc_) && (end_pc_ == rhs.end_pc_);
                }

                inline bool operator<=(const RangeKey& rhs) const {
                    return (*this < rhs) || (*this == rhs);
                }

                inline bool operator>=(const RangeKey& rhs) const {
                    return (*this > rhs) || (*this == rhs);
                }
        };

        using DwarfInlineMap = std::unordered_map<Dwarf_Off, std::string>;

    public:
        class Symbol {
            private:
                bool valid_ = false;
                bool inlined_ = false;
                std::string name_;
                RangeVector ranges_;

                void checkInline_(const Dwarf_Die die) {
                    Dwarf_Error err = 0;
                    Dwarf_Half tag;

                    int res = dwarf_tag(die, &tag, &err);
                    DWARF_ASSERT_OK("Could not get CU tag");

                    inlined_ = tag == DW_TAG_inlined_subroutine;
                }


                void initPCs_(const Dwarf_Die die) {
                    Dwarf_Addr low_pc = 0;
                    Dwarf_Addr high_pc = 0;
                    Dwarf_Error err = 0;
                    int res = dwarf_lowpc(die, &low_pc, &err);
                    DWARF_ASSERT_NOERR("Failed getting low PC");

                    if(res != DW_DLV_OK) {
                        return;
                    }

                    Dwarf_Half form = 0;
                    enum Dwarf_Form_Class formclass = DW_FORM_CLASS_UNKNOWN;

                    res = dwarf_highpc_b(die,
                                         &high_pc,
                                         &form,
                                         &formclass,
                                         &err);
                    DWARF_ASSERT_NOERR("Failed getting high PC");

                    if(res == DW_DLV_OK) {
                        if (form != DW_FORM_CLASS_ADDRESS && !dwarf_addr_form_is_indexed(form)) {
                            high_pc += low_pc + (high_pc == 0);
                        }
                    }
                    else {
                        high_pc = low_pc + 1;
                    }

                    ranges_.emplace_back(low_pc, high_pc);
                    valid_ = true;
                }

            public:
                static inline std::string demangleName(const char* name_cstr) {
                    return boost::core::demangle(name_cstr);
                }

                Symbol(const Dwarf_Die die, const DwarfInlineMap& inline_map) {
                    checkInline_(die);
                    Dwarf_Error err = 0;
                    Dwarf_Attribute abstract_origin_attr;
                    int res = dwarf_attr(die, DW_AT_abstract_origin, &abstract_origin_attr, &err);
                    DWARF_ASSERT_NOERR("Failed checking abstract origin status");

                    if(res != DW_DLV_OK) {
                        return;
                    }

                    Dwarf_Off die_global_offset;
                    Dwarf_Bool is_info = DWARF_TRUE;
                    res = dwarf_global_formref_b(abstract_origin_attr,
                                                 &die_global_offset,
                                                 &is_info,
                                                 &err);
                    DWARF_ASSERT_OK("Failed getting inlining value");

                    name_ = inline_map.at(die_global_offset);
                    initPCs_(die);
                }

                Symbol(const Dwarf_Die die,
                       const RangeVector& ranges) {
                    checkInline_(die);
                    Dwarf_Error err = 0;
                    char* name_cstr;
                    int res = dwarf_diename(die, &name_cstr, &err);
                    if(res != DW_DLV_OK) {
                        return;
                    }

                    name_ = demangleName(name_cstr);
                    ranges_ = ranges;
                    valid_ = true;
                }

                explicit Symbol(const Dwarf_Die die) {
                    checkInline_(die);
                    Dwarf_Error err = 0;
                    char* name_cstr;
                    int res = dwarf_diename(die, &name_cstr, &err);
                    if(res != DW_DLV_OK) {
                        return;
                    }

                    name_ = demangleName(name_cstr);
                    initPCs_(die);
                }

                Symbol(const ELFIO::symbol_section_accessor& symbols, const unsigned int index) {
                    ELFIO::Elf64_Addr value = 0;
                    ELFIO::Elf_Xword size = 0;
                    unsigned char bind = 0;
                    unsigned char type = 0;
                    ELFIO::Elf_Half section_index = 0;
                    unsigned char other = 0;

                    valid_ = symbols.get_symbol(index,
                                                name_,
                                                value,
                                                size,
                                                bind,
                                                type,
                                                section_index,
                                                other) &&
                             !name_.empty();

                    if(valid_) {
                        size = size ? size : 1; // If the symbol has 0 size, override to 1
                        ranges_.emplace_back(value, value + size);
                    }
                }

                const auto& getRanges() const {
                    return ranges_;
                }

                bool inlined() const {
                    return inlined_;
                }

                bool checkPC(const uint64_t pc) const {
                    auto it = std::lower_bound(
                        ranges_.begin(),
                        ranges_.end(),
                        pc,
                        [](const decltype(ranges_)::value_type& elem,
                           const uint64_t val) { return elem.first < val; }
                    );

                    if(it == ranges_.end() || it->first > pc) {
                        --it;
                    }

                    return it->second > pc;
                }

                const std::string& name() const {
                    return name_;
                }

                bool valid() const {
                    return valid_;
                }
        };

        using SymbolHandle = std::shared_ptr<Symbol>;

    private:
        class SymbolTable {
            public:
            private:
                std::multimap<RangeKey, SymbolHandle> symbols_;

            public:
                void initFromELFIO(const ELFIO::elfio& reader) {
                    for(unsigned int i = 0; i < reader.sections.size(); ++i) {
                        auto* section = reader.sections[i];
                        if(STF_EXPECT_FALSE(section->get_type() == SHT_SYMTAB)) {
                            const ELFIO::symbol_section_accessor symbols(reader, section);
                            for(unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
                                auto new_symbol = std::make_shared<Symbol>(symbols, j);
                                if(STF_EXPECT_TRUE(new_symbol->valid())) {
                                    symbols_.emplace(new_symbol->getRanges().front(), std::move(new_symbol));
                                }
                            }
                        }
                    }
                }

                /**
                 * Finds the function containing the specified address
                 */
                SymbolHandle findFunction(const uint64_t address) const {
                    const RangeKey key(address, address + 1);
                    auto it = symbols_.lower_bound(key);

                    if(it == symbols_.end() || it->first.startsAfter(key)) {
                        if(it == symbols_.begin()) {
                            return nullptr;
                        }
                        --it;
                    }

                    auto end_it = std::next(it);

                    while(it != symbols_.begin() && std::prev(it)->first >= key) {
                        --it;
                    }

                    // The map is sorted from smallest to largest range, so we
                    // can just return the first one that contains the address
                    for(; it != end_it; ++it) {
                        const auto& symbol = it->second;
                        if(symbol->checkPC(address)) {
                            return symbol;
                        }
                    }

                    return nullptr;
                }

                template<typename ... Args>
                inline auto emplace(const RangeKey& key, Args&&... args) {
                    return symbols_.emplace(key, std::forward<Args>(args)...);
                }

                template<typename ... Args>
                inline static auto makeSymbol(Args&&... args) {
                    return std::make_shared<Symbol>(std::forward<Args>(args)...);
                }
        };

        class DwarfReader {
            private:
                template<typename... Args>
                SymbolHandle emplaceSymbol_(SymbolTable& symbol_table, Args&&... args) {
                    auto new_symbol = SymbolTable::makeSymbol(std::forward<Args>(args)...);
                    stf_assert(new_symbol, "Failed to allocate new symbol");

                    if(new_symbol->valid()) {
                        for(const auto& p: new_symbol->getRanges()) {
                            symbol_table.emplace(RangeKey(p), new_symbol);
                        }
                        return new_symbol;
                    }
                    return nullptr;
                }

                template<typename... Args>
                void emplaceOrTryRangeSymbol_(SymbolTable& symbol_table,
                                              const Dwarf_Debug dbg,
                                              const Dwarf_Die die,
                                              Args&&... args) {
                    if(!emplaceSymbol_(symbol_table, die, std::forward<Args>(args)...)) {
                        handleRanges_(symbol_table, dbg, die);
                    }
                }

                void handleRanges_(SymbolTable& symbol_table,
                                   const Dwarf_Debug dbg,
                                   const Dwarf_Die die) {
                    Dwarf_Error err = 0;
                    Dwarf_Attribute range_attr;
                    int res = dwarf_attr(die, DW_AT_ranges, &range_attr, &err);
                    DWARF_ASSERT_NOERR("Failed getting range attr");

                    if(res != DW_DLV_OK) {
                        return;
                    }

                    Dwarf_Half form;
                    res = dwarf_whatform(range_attr, &form, &err);
                    DWARF_ASSERT_NOERR("Failed getting range form");

                    if (res != DW_DLV_OK) {
                        return;
                    }

                    static constexpr Dwarf_Half DWVERSION5 = 5;
                    Dwarf_Half cu_version = 2;
                    Dwarf_Half cu_offset_size = 4;
                    Dwarf_Unsigned original_off = 0;

                    res = dwarf_get_version_of_die(die, &cu_version, &cu_offset_size);
                    DWARF_ASSERT_NOERR("Failed getting DIE version");

                    if(form == DW_FORM_rnglistx) {
                        res = dwarf_formudata(range_attr, &original_off, &err);
                        DWARF_ASSERT_NOERR("Failed getting DW_FORM_rnglistx offset");
                    }
                    else {
                        res = dwarf_global_formref(range_attr, &original_off, &err);
                        DWARF_ASSERT_NOERR("Failed getting range offset");
                    }

                    if(res == DW_DLV_NO_ENTRY) {
                        return;
                    }

                    RangeVector ranges;

                    if(cu_version < DWVERSION5) {
                        Dwarf_Ranges *rangeset = 0;
                        Dwarf_Signed rangecount = 0;
                        Dwarf_Unsigned bytecount = 0;
                        Dwarf_Unsigned realoffset = 0;
                        res = dwarf_get_ranges_b(dbg,
                                                 original_off,
                                                 die,
                                                 &realoffset,
                                                 &rangeset,
                                                 &rangecount,
                                                 &bytecount,
                                                 &err);
                        DWARF_ASSERT_NOERR("Failed getting range value");
                        if(STF_EXPECT_FALSE(res == DW_DLV_NO_ENTRY)) {
                            return;
                        }

                        Dwarf_Off die_glb_offset = 0;
                        Dwarf_Off die_off = 0;
                        res = dwarf_die_offsets(die,
                                                &die_glb_offset,
                                                &die_off,
                                                &err);
                        if(STF_EXPECT_FALSE(res == DW_DLV_ERROR)) {
                            dwarf_dealloc_ranges(dbg,rangeset,rangecount);
                            stf_throw(DWARF_ERROR("Failed getting range value offsets"));
                        }

                        if(STF_EXPECT_FALSE(res == DW_DLV_NO_ENTRY)) {
                            return;
                        }

                        Dwarf_Addr base_addr = 0;
                        for (int i = 0; i < rangecount; ++i) {
                            Dwarf_Addr start_addr = 0;
                            Dwarf_Addr end_addr = 0;
                            Dwarf_Ranges * r = rangeset + i;
                            switch(r->dwr_type) {
                                case DW_RANGES_ENTRY:
                                    if(STF_EXPECT_FALSE(r->dwr_addr1 == r->dwr_addr2)) {
                                        continue;
                                    }

                                    start_addr = r->dwr_addr1 + base_addr;
                                    end_addr = r->dwr_addr2 + base_addr;
                                    break;
                                case DW_RANGES_ADDRESS_SELECTION:
                                    base_addr = r->dwr_addr2;
                                    break;
                                case DW_RANGES_END:
                                    break;
                            }

                            ranges.emplace_back(start_addr, end_addr);
                        }

                        dwarf_dealloc_ranges(dbg, rangeset, rangecount);
                    }
                    else {
                        Dwarf_Unsigned global_offset_of_rle_set = 0;
                        Dwarf_Unsigned count_rnglists_entries = 0;
                        Dwarf_Rnglists_Head rnglhead = 0;

                        res = dwarf_rnglists_get_rle_head(range_attr,
                                                          form,
                                                          original_off,
                                                          &rnglhead,
                                                          &count_rnglists_entries,
                                                          &global_offset_of_rle_set,
                                                          &err);
                        DWARF_ASSERT_NOERR("Failed getting range list head");

                        if(STF_EXPECT_FALSE(res == DW_DLV_NO_ENTRY)) {
                            return;
                        }

                        Dwarf_Unsigned i = 0;
                        Dwarf_Bool no_debug_addr_available = DWARF_FALSE;

                        for ( ; i < count_rnglists_entries; ++i) {
                            unsigned rle_code = 0;

                            Dwarf_Addr start_addr = 0;
                            Dwarf_Addr end_addr = 0;

                            res  = dwarf_get_rnglists_entry_fields_a(rnglhead,
                                                                     i,
                                                                     nullptr,
                                                                     &rle_code,
                                                                     nullptr,
                                                                     nullptr,
                                                                     &no_debug_addr_available,
                                                                     &start_addr,
                                                                     &end_addr,
                                                                     &err);

                            DWARF_ASSERT_NOERR("Failed getting range list entry");

                            if (STF_EXPECT_FALSE(no_debug_addr_available || res == DW_DLV_NO_ENTRY)) {
                                return;
                            }

                            if(STF_EXPECT_FALSE(rle_code == DW_RLE_end_of_list)) {
                                break;
                            }

                            switch(rle_code) {
                                case DW_RLE_base_address:
                                case DW_RLE_base_addressx:
                                    continue;

                                case DW_RLE_startx_endx:
                                case DW_RLE_startx_length:
                                case DW_RLE_offset_pair:
                                case DW_RLE_start_end:
                                case DW_RLE_start_length:
                                    break;
                            }

                            if(STF_EXPECT_FALSE(start_addr == end_addr)) {
                                continue;
                            }

                            ranges.emplace_back(start_addr, end_addr);
                        }

                        dwarf_dealloc_rnglists_head(rnglhead);
                    }

                    std::sort(ranges.begin(),
                              ranges.end(),
                              [](const decltype(ranges)::value_type& lhs,
                                 const decltype(ranges)::value_type& rhs) {return lhs.first < rhs.first;});
                    emplaceSymbol_(symbol_table, die, ranges);
                }

                void addCU_(SymbolTable& symbol_table,
                            const Dwarf_Debug dbg,
                            const Dwarf_Die die,
                            DwarfInlineMap& inlined_cus,
                            std::vector<Dwarf_Off>& unresolved_inlines) {
                    Dwarf_Error err = 0;
                    Dwarf_Half tag;

                    int res = dwarf_tag(die, &tag, &err);
                    DWARF_ASSERT_OK("Could not get CU tag");

                    if(tag == DW_TAG_subprogram) {
                        Dwarf_Attribute inline_attr;
                        res = dwarf_attr(die, DW_AT_inline, &inline_attr, &err);
                        DWARF_ASSERT_NOERR("Failed checking inlining status");

                        Dwarf_Signed inlined = DWARF_FALSE;
                        if(res == DW_DLV_OK) {
                            res = dwarf_formsdata(inline_attr, &inlined, &err);
                            DWARF_ASSERT_OK("Failed getting inlining value");
                        }

                        if(inlined == DW_INL_inlined || inlined == DW_INL_declared_inlined) {
                            Dwarf_Off die_global_offset;
                            res = dwarf_dieoffset(die, &die_global_offset, &err);
                            DWARF_ASSERT_OK("Couldn't get offset for inlined DIE");

                            char* name_cstr;
                            res = dwarf_diename(die, &name_cstr, &err);
                            DWARF_ASSERT_OK("Couldn't get name for inlined function");

                            inlined_cus[die_global_offset] = Symbol::demangleName(name_cstr);
                        }
                        else {
                            emplaceOrTryRangeSymbol_(symbol_table, dbg, die);
                        }
                    }
                    else if(tag == DW_TAG_inlined_subroutine) {
                        Dwarf_Off die_global_offset = 0;
                        res = dwarf_dieoffset(die, &die_global_offset, &err);
                        DWARF_ASSERT_OK("Couldn't get offset for inlined function");

                        unresolved_inlines.emplace_back(die_global_offset);
                    }
                }

                void iterateSiblings_(SymbolTable& symbol_table,
                                      const Dwarf_Debug dbg,
                                      const Dwarf_Die in_die,
                                      const int is_info,
                                      const int in_level,
                                      DwarfInlineMap& inlined_cus,
                                      std::vector<Dwarf_Off>& unresolved_inlines) {
                    Dwarf_Die cur_die = in_die;
                    Dwarf_Die child = 0;
                    Dwarf_Error err = 0;

                    addCU_(symbol_table, dbg, in_die, inlined_cus, unresolved_inlines);

                    /*   Loop on a list of siblings */
                    while(true) {
                        Dwarf_Die sib_die = 0;

                        /*  Depending on your goals, the in_level,
                            and the DW_TAG of cur_die, you may want
                            to skip the dwarf_child call. */
                        int res = dwarf_child(cur_die, &child, &err);
                        DWARF_ASSERT_NOERR("Error in dwarf_child, level " << in_level);

                        if(res == DW_DLV_OK) {
                            iterateSiblings_(symbol_table,
                                             dbg,
                                             child,
                                             is_info,
                                             in_level + 1,
                                             inlined_cus,
                                             unresolved_inlines);
                            /* No longer need 'child' die. */
                            dwarf_dealloc(dbg, child, DW_DLA_DIE);
                            child = 0;
                        }

                        /* res == DW_DLV_NO_ENTRY or DW_DLV_OK */
                        res = dwarf_siblingof_b(dbg, cur_die, is_info, &sib_die, &err);
                        DWARF_ASSERT_NOERR("Error in dwarf_siblingof_b");

                        if(res == DW_DLV_NO_ENTRY) {
                            /* Done at this level. */
                            break;
                        }

                        /* res == DW_DLV_OK */
                        if(cur_die != in_die) {
                            dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);
                            cur_die = 0;
                        }
                        cur_die = sib_die;
                        addCU_(symbol_table, dbg, sib_die, inlined_cus, unresolved_inlines);
                    }
                }

            public:
                void populate(const std::string& filename, SymbolTable& symbol_table) {
                    Dwarf_Debug dbg = 0;
                    Dwarf_Error err = 0;
                    char pathbuf[FILENAME_MAX];

                    int res = dwarf_init_path(filename.c_str(),
                                              pathbuf,
                                              sizeof(pathbuf),
                                              DW_GROUPNUMBER_ANY,
                                              nullptr,
                                              nullptr,
                                              &dbg,
                                              &err);

                    if(STF_EXPECT_FALSE(res == DW_DLV_ERROR)) {
                        dwarf_dealloc_error(dbg, err);
                        stf_throw("Failed to open " << filename << " with DWARF parser");
                    }

                    if(STF_EXPECT_FALSE(res == DW_DLV_NO_ENTRY)) {
                        return;
                    }

                    Dwarf_Unsigned abbrev_offset = 0;
                    Dwarf_Half     address_size = 0;
                    Dwarf_Half     version_stamp = 0;
                    Dwarf_Half     offset_size = 0;
                    Dwarf_Half     extension_size = 0;
                    Dwarf_Sig8     signature;
                    Dwarf_Unsigned typeoffset = 0;
                    Dwarf_Unsigned next_cu_header = 0;
                    Dwarf_Half     header_cu_type = 0;
                    Dwarf_Bool     is_info = DWARF_TRUE;

                    try {
                        DwarfInlineMap inlined_cus;
                        std::vector<Dwarf_Off> unresolved_inlines;

                        while(true) {
                            Dwarf_Die no_die = 0;
                            Dwarf_Die cu_die = 0;
                            Dwarf_Unsigned cu_header_length = 0;

                            memset(&signature, 0, sizeof(signature));
                            res = dwarf_next_cu_header_d(dbg,
                                                         is_info,
                                                         &cu_header_length,
                                                         &version_stamp,
                                                         &abbrev_offset,
                                                         &address_size,
                                                         &offset_size,
                                                         &extension_size,
                                                         &signature,
                                                         &typeoffset,
                                                         &next_cu_header,
                                                         &header_cu_type,
                                                         &err);

                            DWARF_ASSERT_NOERR("Error reading CUs from " << filename);

                            if (res == DW_DLV_NO_ENTRY) {
                                break;
                            }

                            /* The CU will have a single sibling, a cu_die. */
                            res = dwarf_siblingof_b(dbg,
                                                    no_die,
                                                    is_info,
                                                    &cu_die,
                                                    &err);

                            DWARF_ASSERT_OK("Error reading CU siblings from " << filename);

                            iterateSiblings_(symbol_table, dbg, cu_die, is_info, 0, inlined_cus, unresolved_inlines);
                        }

                        for(const auto inlined_die_offset: unresolved_inlines) {
                            Dwarf_Die die;
                            res = dwarf_offdie_b(dbg, inlined_die_offset, DWARF_TRUE, &die, &err);
                            DWARF_ASSERT_OK("Error reading DIE from offset " << std::hex << inlined_die_offset);

                            emplaceOrTryRangeSymbol_(symbol_table, dbg, die, inlined_cus);
                            dwarf_dealloc_die(die);
                        }
                    }
                    catch(const stf::STFException&) {
                        dwarf_finish(dbg);
                        throw;
                    }

                    dwarf_finish(dbg);
                }
        };

        ELFIO::elfio reader_;
        std::map<RangeKey, const ELFIO::segment*> segments_;
        fs::path filename_;
        SymbolTable symbols_;

        auto findSection_(const uint64_t address, const size_t num_bytes = 0) const {
            validateAddress_(address);

            const uint64_t end_address = address + num_bytes;
            const RangeKey key(address, end_address);
            auto it = segments_.lower_bound(key);
            if(it == segments_.end()) {
                --it;
            }

            if(it->first.startAddress() > address) {
                --it;
            }

            if(address < it->first.startAddress() || address >= it->first.endAddress()) {
                return segments_.end();
            }

            return it;
        }

        void read_(const uint64_t address, void* const ptr, const size_t num_bytes) const override {
            auto it = findSection_(address, num_bytes);

            stf_assert(it != segments_.end(),
                       "Failed to find address " << std::hex << address);

            memcpy(reinterpret_cast<uint8_t*>(ptr),
                   it->second->get_data() + address - it->first.startAddress(),
                   num_bytes);
        }

    public:
        explicit STFElf() :
            STFBinary(0)
        {
        }

        void open(const std::string_view filename) final {
            filename_ = filename;

            stf_assert(reader_.load(filename.data()), "Failed to load ELF " << filename);
            for(unsigned int i = 0; i < reader_.segments.size(); ++i) {
                const auto* segment = reader_.segments[i];
                if(segment->get_type() == PT_LOAD) {
                    const auto start_addr = segment->get_virtual_address();
                    const auto end_addr = start_addr + segment->get_memory_size();
                    segments_.emplace(std::piecewise_construct,
                                      std::forward_as_tuple(start_addr, end_addr),
                                      std::forward_as_tuple(segment));
                    file_size_ = std::max(file_size_, static_cast<size_t>(end_addr));
                }
            }
        }

        inline void readSymbols() {
            // Try to populate with DWARF info first
            DwarfReader dwarf_reader;
            dwarf_reader.populate(filename_, symbols_);

            // Populate with regular symbols next - if DWARF info isn't present or doesn't cover
            // an address, it should fall back to the ELF symbols
            symbols_.initFromELFIO(reader_);
        }

        inline SymbolHandle findFunction(const uint64_t pc) const {
            return symbols_.findFunction(pc);
        }
};

#undef DWARF_ERROR
#undef DWARF_ASSERT_OK
#undef DWARF_ASSERT_NOERR

