#include <map>
#include <set>
#include "stf_symbol_table.hpp"
#include "print_utils.hpp"

class SymbolHistogram {
    private:
        static inline constexpr std::string_view INLINE_SUFFIX = " (inlined)";

        STFSymbolTable symbol_table_;
        std::unordered_map<const STFSymbol*, uint64_t> symbol_counts_;

    public:
        explicit SymbolHistogram(const std::string& elf) :
            symbol_table_(elf)
        {
            stf_assert(!symbol_table_.empty(), elf << " does not contain any symbol information!");
        }

        inline bool validPC(const uint64_t pc) const {
            return symbol_table_.validPC(pc);
        }

        inline void count(const uint64_t pc) {
            const auto result = symbol_table_.findFunction(pc);

            if(STF_EXPECT_FALSE(result.first && (symbol_counts_.empty() || result.second))) {
                    ++symbol_counts_[result.first.get()];
            }
        }

        static inline std::pair<uint64_t, std::set<uint64_t>>& getConsolidatedEntry_(std::map<std::string, std::pair<uint64_t, std::set<uint64_t>>>& consolidated_counts, const STFSymbol& symbol) {
            if(symbol.inlined()) {
                return consolidated_counts[symbol.name() + std::string(INLINE_SUFFIX)];
            }

            return consolidated_counts[symbol.name()];
        }

        inline void dump() const {
            size_t max_symbol_length = 0;

            std::map<std::string, std::pair<uint64_t, std::set<uint64_t>>> consolidated_counts;

            for(const auto& symbol_pair: symbol_counts_) {
                const auto& symbol = *symbol_pair.first;
                auto& entry = getConsolidatedEntry_(consolidated_counts, symbol);
                entry.first += symbol_pair.second;
                for(const auto& range: symbol.getRanges()) {
                    entry.second.emplace(range.startAddress());
                }
            }

            std::multimap<uint64_t, std::pair<std::string, std::set<uint64_t>>> sorted_counts;

            for(const auto& symbol_pair: consolidated_counts) {
                sorted_counts.emplace(symbol_pair.second.first, std::make_pair(symbol_pair.first, symbol_pair.second.second));
                max_symbol_length = std::max(max_symbol_length, symbol_pair.first.size());
            }

            const std::string entry_sep(max_symbol_length + 4 + 16 + 4 + 16, '-');
            stf::print_utils::printLeft("Function", static_cast<int>(max_symbol_length));
            stf::print_utils::printSpaces(4);
            stf::print_utils::printLeft("Address", 16);
            stf::print_utils::printSpaces(4);
            stf::print_utils::printWidth("# Calls", 16);
            std::cout << std::endl << entry_sep << std::endl;

            for(auto it = sorted_counts.rbegin(); it != sorted_counts.rend(); ++it) {
                stf::print_utils::printLeft(it->second.first, static_cast<int>(max_symbol_length));
                stf::print_utils::printSpaces(4);

                const auto& address_ranges = it->second.second;
                auto range_it = address_ranges.begin();
                stf::print_utils::printHex(*range_it);
                stf::print_utils::printSpaces(4);
                stf::print_utils::printDec(it->first, 16, ' ');
                std::cout << std::endl;

                for(++range_it; range_it != address_ranges.end(); ++range_it) {
                    stf::print_utils::printSpaces(static_cast<size_t>(max_symbol_length) + 4);
                    stf::print_utils::printHex(*range_it);
                    std::cout << std::endl;
                }
                std::cout << entry_sep << std::endl;
            }
        }
};


