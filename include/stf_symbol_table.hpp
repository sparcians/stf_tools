#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "stf_dwarf.hpp"
#include "stf_elf.hpp"

class STFSymbol {
    public:
        using DwarfInlineMap = std::unordered_map<uint64_t, std::string>;

        class InvalidSymbolException : public std::exception {
        };

    private:
        const bool inlined_ = false;
        const std::string name_;
        const std::vector<STFAddressRange> ranges_;

        static inline STFAddressRange initSingleRange_(const STFDwarf::Die& die) {
            const auto low_pc = die.getLowPC();

            if(!low_pc) {
                throw InvalidSymbolException();
            }

            const uint64_t high_pc = die.getHighPC(*low_pc);
            return STFAddressRange(*low_pc, high_pc);
        }

        static inline std::string getName_(const STFDwarf::Die& die) {
            if(auto name_result = die.getName(); STF_EXPECT_TRUE(name_result)) {
                return *name_result;
            }

            throw InvalidSymbolException();
        }

        static inline std::string getName_(const STFDwarf::Die& die,
                                           const DwarfInlineMap& inline_map) {
            if(const auto inline_offset = die.getInlineOffset(); STF_EXPECT_TRUE(inline_offset)) {
                return inline_map.at(*inline_offset);
            }

            throw InvalidSymbolException();
        }

        static inline std::tuple<std::string, STFAddressRange> initFromELF_(const ELFIO::symbol_section_accessor& symbols, const unsigned int index) {
            ELFIO::Elf64_Addr value = 0;
            ELFIO::Elf_Xword size = 0;
            unsigned char bind = 0;
            unsigned char type = 0;
            ELFIO::Elf_Half section_index = 0;
            unsigned char other = 0;
            std::string mangled_name;

            if(STF_EXPECT_FALSE(!symbols.get_symbol(index,
                                                    mangled_name,
                                                    value,
                                                    size,
                                                    bind,
                                                    type,
                                                    section_index,
                                                    other) || mangled_name.empty())) {
                throw InvalidSymbolException();
            }

            size = std::max(size, static_cast<decltype(size)>(1)); // If the symbol has 0 size, override to 1
            return std::make_tuple(boost::core::demangle(mangled_name.c_str()),
                                   STFAddressRange(value, value + size));
        }

        STFSymbol(std::string&& name, std::vector<STFAddressRange>&& range, const bool inlined = false) :
            inlined_(inlined),
            name_(std::move(name)),
            ranges_(std::move(range))
        {
            // Ignore empty symbols
            // RISC-V and ARM also define mapping symbols ($d, $x, $t, etc.) that should be ignored
            if(STF_EXPECT_FALSE(name_.empty() || (name_.size() == 2 && name_[0] == '$'))) {
                throw InvalidSymbolException();
            }
        }

        STFSymbol(std::string&& name, STFAddressRange&& range, const bool inlined = false) :
            STFSymbol(std::forward<std::string>(name),
                      std::vector<STFAddressRange>(1, range),
                      inlined)
        {
        }

        explicit STFSymbol(std::tuple<std::string, STFAddressRange>&& tuple_args) :
            STFSymbol(std::forward<std::string>(std::get<0>(tuple_args)),
                      std::forward<STFAddressRange>(std::get<1>(tuple_args)))
        {
        }

        STFSymbol(const STFDwarf::Die& die, std::string&& name) :
            STFSymbol(std::forward<std::string>(name),
                      initSingleRange_(die),
                      die.isInlinedSubroutine())
        {
        }

    public:
        using Handle = std::shared_ptr<STFSymbol>;

        STFSymbol(const STFDwarf::Die& die, const DwarfInlineMap& inline_map) :
            STFSymbol(die, getName_(die, inline_map))
        {
        }

        STFSymbol(const STFDwarf::Die& die,
                  std::vector<STFAddressRange>&& ranges) :
            STFSymbol(getName_(die),
                      std::forward<std::vector<STFAddressRange>>(ranges),
                      die.isInlinedSubroutine())
        {
        }

        explicit STFSymbol(const STFDwarf::Die& die) :
            STFSymbol(die, getName_(die))
        {
        }

        STFSymbol(const ELFIO::symbol_section_accessor& symbols, const unsigned int index) :
            STFSymbol(initFromELF_(symbols, index))
        {
        }

        inline const auto& getRanges() const {
            return ranges_;
        }

        inline bool inlined() const {
            return inlined_;
        }

        inline bool checkPC(const uint64_t pc) const {
            auto it = std::lower_bound(
                ranges_.begin(),
                ranges_.end(),
                pc
            );

            if(it == ranges_.end() || *it > pc) {
                --it;
            }

            return it->contains(pc);
        }

        inline const std::string& name() const {
            return name_;
        }
};

class STFSymbolTable {
    private:
        std::multimap<STFAddressRange, STFSymbol::Handle> symbols_;

        template<typename... Args>
        inline bool emplaceSymbol_(Args&&... args) {
            try {
                auto new_symbol = std::make_shared<STFSymbol>(std::forward<Args>(args)...);
                stf_assert(new_symbol, "Failed to allocate new symbol");

                for(const auto& p: new_symbol->getRanges()) {
                    symbols_.emplace(p, new_symbol);
                }
            }
            catch(const STFSymbol::InvalidSymbolException&) {
                return false;
            }
            return true;
        }

        template<typename... Args>
        inline void emplaceSymbolFromDwarf_(const STFDwarf::Die& die,
                                            Args&&... args) {
            if(!emplaceSymbol_(die, std::forward<Args>(args)...)) {
                auto ranges = die.getRanges();
                if(!ranges.empty()) {
                    std::sort(ranges.begin(), ranges.end());
                    emplaceSymbol_(die, std::move(ranges));
                }
            }
        }

        inline void populateDwarf_(const std::string& filename) {
            try {
                STFDwarf dwarf(filename);

                STFSymbol::DwarfInlineMap inlined_cus;
                std::vector<uint64_t> unresolved_inlines;

                dwarf.iterateDies(
                    [this, &inlined_cus, &unresolved_inlines](const STFDwarf::Die& die) {
                        if(die.isSubprogram()) {
                            if(die.isInlined()) {
                                const auto die_name = die.getName();
                                stf_assert(die_name, "Couldn't get name for inlined function");

                                inlined_cus[die.getOffset()] = *die_name;
                            }
                            else {
                                emplaceSymbolFromDwarf_(die);
                            }
                        }
                        else if(die.isInlinedSubroutine()) {
                            unresolved_inlines.emplace_back(die.getOffset());
                        }
                    }
                );

                for(const auto inlined_die_offset: unresolved_inlines) {
                    emplaceSymbolFromDwarf_(dwarf.getDieFromOffset(inlined_die_offset), inlined_cus);
                }
            }
            catch(const STFDwarf::NoDwarfInfoException&) {
            }
        }

    public:
        explicit STFSymbolTable(const STFElf& elf) {
            // Try to populate with DWARF info first
            populateDwarf_(elf.getFilename());

            const auto& elf_reader = elf.getReader();

            for(unsigned int i = 0; i < elf_reader.sections.size(); ++i) {
                auto* section = elf_reader.sections[i];
                if(STF_EXPECT_FALSE(section->get_type() == ELFIO::SHT_SYMTAB)) {
                    const ELFIO::symbol_section_accessor symbols(elf_reader, section);
                    for(unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
                        try {
                            auto new_symbol = std::make_shared<STFSymbol>(symbols, j);
                            symbols_.emplace(new_symbol->getRanges().front(), std::move(new_symbol));
                        }
                        catch(const STFSymbol::InvalidSymbolException&) {
                        }
                    }
                }
            }
        }

        explicit STFSymbolTable(const std::string& filename) :
            STFSymbolTable(STFElf(filename))
        {
        }

        /**
         * Finds the function containing the specified address
         */
        inline STFSymbol::Handle findFunction(const uint64_t address) const {
            const STFAddressRange key(address, address + 1);
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
};
