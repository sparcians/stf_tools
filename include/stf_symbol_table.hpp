#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/core/demangle.hpp>

#include "stf_dwarf.hpp"
#include "stf_elf.hpp"

class STFSymbol {
    public:
        using DwarfInlineMap = std::unordered_map<uint64_t, std::string>;

    private:
        const bool valid_ = true;
        const bool inlined_ = false;
        const std::string name_;
        const std::vector<STFAddressRange> ranges_;

        static inline STFAddressRange initSingleRange_(const dwarf_wrapper::Die& die) {
            const auto low_pc = die.getLowPC();

            if(!low_pc) {
                return STFAddressRange::invalid();
            }

            const uint64_t high_pc = die.getHighPC(*low_pc);
            return STFAddressRange(*low_pc, high_pc);
        }

        static inline std::string getDwarfDieName_(const dwarf_wrapper::Die& die) {
            if(const auto name_result = die.getName(); STF_EXPECT_TRUE(name_result)) {
                return boost::core::demangle(name_result);
            }

            return "";
        }

        static inline std::string getDwarfDieName_(const dwarf_wrapper::Die& die,
                                                   const DwarfInlineMap& inline_map) {
            if(const auto inline_offset = die.getInlineOffset(); STF_EXPECT_TRUE(inline_offset)) {
                return inline_map.at(*inline_offset);
            }

            return "";
        }

        static inline STFSymbol makeFromELF_(const ELFIO::symbol_section_accessor& symbols,
                                             const unsigned int index) {
            ELFIO::Elf64_Addr value = 0;
            ELFIO::Elf_Xword size = 0;
            unsigned char bind = 0;
            unsigned char type = 0;
            ELFIO::Elf_Half section_index = 0;
            unsigned char other = 0;
            std::string symbol_name;

            if(STF_EXPECT_FALSE(!symbols.get_symbol(index,
                                                    symbol_name,
                                                    value,
                                                    size,
                                                    bind,
                                                    type,
                                                    section_index,
                                                    other) || !value || symbol_name.empty())) {
                return STFSymbol();
            }

            size = std::max(size, static_cast<decltype(size)>(1)); // If the symbol has 0 size, override to 1
            return STFSymbol(boost::core::demangle(symbol_name.c_str()),
                             STFAddressRange(value, value + size));
        }

        static inline bool checkValid_(const std::string& name, const std::vector<STFAddressRange>& range) {
            // Ignore empty symbols
            // RISC-V and ARM also define mapping symbols ($d, $x, $t, etc.) that should be ignored
            return !(name.empty() ||
                    (name.size() == 2 && name[0] == '$') ||
                    (range.size() == 1 && range.front() == STFAddressRange::invalid()));
        }

        STFSymbol() :
            valid_(false)
        {
        }

        STFSymbol(std::string&& name, std::vector<STFAddressRange>&& range, const bool inlined = false) :
            valid_(checkValid_(name, range)),
            inlined_(inlined),
            name_(std::move(name)),
            ranges_(std::move(range))
        {
        }

        STFSymbol(std::string&& name, STFAddressRange&& range, const bool inlined = false) :
            STFSymbol(std::forward<std::string>(name),
                      std::vector<STFAddressRange>(1, range),
                      inlined)
        {
        }

        STFSymbol(const dwarf_wrapper::Die& die, std::string&& name) :
            STFSymbol(std::forward<std::string>(name),
                      initSingleRange_(die),
                      die.isInlinedSubroutine())
        {
        }

        STFSymbol(const dwarf_wrapper::Die& die, std::string&& name, std::vector<STFAddressRange>&& ranges) :
            STFSymbol(std::forward<std::string>(name),
                      std::forward<std::vector<STFAddressRange>>(ranges),
                      die.isInlinedSubroutine())
        {
        }

    public:
        using Handle = std::shared_ptr<STFSymbol>;

        STFSymbol(const dwarf_wrapper::Die& die, const DwarfInlineMap& inline_map) :
            STFSymbol(die, getDwarfDieName_(die, inline_map))
        {
        }

        STFSymbol(const dwarf_wrapper::Die& die,
                  const DwarfInlineMap& inline_map,
                  std::vector<STFAddressRange>&& ranges) :
            STFSymbol(die,
                      getDwarfDieName_(die, inline_map),
                      std::forward<std::vector<STFAddressRange>>(ranges))
        {
        }

        STFSymbol(const dwarf_wrapper::Die& die, std::vector<STFAddressRange>&& ranges) :
            STFSymbol(die,
                      getDwarfDieName_(die),
                      std::forward<std::vector<STFAddressRange>>(ranges))
        {
        }

        explicit STFSymbol(const dwarf_wrapper::Die& die) :
            STFSymbol(die, getDwarfDieName_(die))
        {
        }

        STFSymbol(const ELFIO::symbol_section_accessor& symbols, const unsigned int index) :
            STFSymbol(makeFromELF_(symbols, index))
        {
        }

        inline const auto& getRanges() const {
            return ranges_;
        }

        inline bool inlined() const {
            return inlined_;
        }

        inline bool checkPC(const uint64_t pc) const {
            const STFAddressRange key(pc);
            auto it = std::upper_bound(
                ranges_.begin(),
                ranges_.end(),
                STFAddressRange(pc)
            );

            while(it != ranges_.begin() && (it == ranges_.end() || std::prev(it)->contains(pc))) {
                --it;
            }

            if(!it->contains(key)) {
                stf_assert(it->startsAfter(key));
                bool found = false;
                for(auto search_it = std::next(it); search_it != ranges_.end(); ++search_it) {
                    if(search_it->contains(key)) {
                        it = search_it;
                        found = true;
                        break;
                    }
                }

                if(STF_EXPECT_FALSE(!found)) {
                    return false;
                }
            }

            return it->contains(pc);
        }

        inline const std::string& name() const {
            return name_;
        }

        inline operator bool() const {
            return valid_;
        }
};

class STFSymbolTable {
    private:
        std::multimap<STFAddressRange, STFSymbol::Handle> symbols_;
        uint64_t elf_min_address_ = 0;
        uint64_t elf_max_address_ = 0;

        template<typename... Args>
        inline bool emplaceSymbol_(Args&&... args) {
            auto new_symbol = std::make_shared<STFSymbol>(std::forward<Args>(args)...);

            if(*new_symbol) {
                for(const auto& p: new_symbol->getRanges()) {
                    symbols_.emplace(p, new_symbol);
                }

                return true;
            }

            return false;
        }

        template<typename... Args>
        inline void emplaceSymbolFromDwarf_(const dwarf_wrapper::Die& die, Args&&... args) {
            if(!emplaceSymbol_(die, std::forward<Args>(args)...)) {
                auto ranges = die.getRanges();
                if(!ranges.empty()) {
                    std::sort(ranges.begin(), ranges.end());
                    stf_assert(emplaceSymbol_(die, std::forward<Args>(args)..., std::move(ranges)),
                               "Failed to create symbol");
                }
            }
        }

    public:
        explicit STFSymbolTable(const STFElf& elf) {
            // Try to populate with DWARF info first
            try {
                STFDwarf dwarf(elf.getFilename());

                STFSymbol::DwarfInlineMap inlined_cus;
                std::vector<std::shared_ptr<dwarf_wrapper::Die>> unresolved_inlines;

                dwarf.iterateDies(
                    [this, &inlined_cus, &unresolved_inlines](const std::shared_ptr<dwarf_wrapper::Die>& die) {
                        if(die->isSubprogram()) {
                            if(die->isInlined()) {
                                const auto die_name = die->getName();
                                stf_assert(die_name, "Couldn't get name for inlined function");

                                inlined_cus[die->getOffset()] = boost::core::demangle(die_name);
                            }
                            else {
                                emplaceSymbolFromDwarf_(*die);
                            }
                        }
                        else if(die->isInlinedSubroutine()) {
                            unresolved_inlines.emplace_back(die);
                        }
                    }
                );

                for(const auto& inlined_die: unresolved_inlines) {
                    emplaceSymbolFromDwarf_(*inlined_die, inlined_cus);
                }
            }
            catch(const dwarf_wrapper::NoDwarfInfoException&) {
            }

            const auto& elf_reader = elf.getReader();
            elf_min_address_ = elf.getMinAddress();
            elf_max_address_ = elf.getMaxAddress();

            for(unsigned int i = 0; i < elf_reader.sections.size(); ++i) {
                auto* section = elf_reader.sections[i];
                if(STF_EXPECT_FALSE(section->get_type() == ELFIO::SHT_SYMTAB)) {
                    const ELFIO::symbol_section_accessor symbols(elf_reader, section);
                    for(unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
                        auto new_symbol = std::make_shared<STFSymbol>(symbols, j);
                        if(*new_symbol) {
                            symbols_.emplace(new_symbol->getRanges().front(), std::move(new_symbol));
                        }
                    }
                }
            }
        }

        explicit STFSymbolTable(const std::string& filename) :
            STFSymbolTable(STFElf(filename))
        {
        }

        inline bool validPC(const uint64_t pc) const {
            return (elf_min_address_ <= pc) && (pc < elf_max_address_);
        }

        /**
         * Finds the function containing the specified address
         */
        inline std::pair<STFSymbol::Handle, bool> findFunction(const uint64_t address) const {
            if(!validPC(address)) {
                return std::make_pair(nullptr, false);
            }

            const STFAddressRange key(address);
            auto it = symbols_.upper_bound(key);

            while(it != symbols_.begin() && (it == symbols_.end() || std::prev(it)->first.contains(key))) {
                --it;
            }

            if(!it->first.contains(key)) {
                stf_assert(it->first.startsAfter(key));
                bool found = false;
                for(auto search_it = std::next(it); search_it != symbols_.end(); ++search_it) {
                    if(search_it->first.contains(key)) {
                        it = search_it;
                        found = true;
                        break;
                    }
                }

                if(STF_EXPECT_FALSE(!found)) {
                    return std::make_pair(nullptr, false);
                }
            }

            if(it->second->checkPC(address)) {
                return std::make_pair(it->second, it->first.startAddress() == address);
            }

            return std::make_pair(nullptr, false);
        }

        inline bool empty() const {
            return symbols_.empty();
        }
};
