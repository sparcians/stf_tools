#include "stf_elf.hpp"
#include "print_utils.hpp"

class SymbolHistogram {
    private:
        static inline constexpr std::string_view INLINE_SUFFIX = " (inlined)";

        STFElf stf_elf_;
        std::unordered_map<const STFElf::Symbol*, uint64_t> symbol_counts_;

    public:
        explicit SymbolHistogram(const std::string& elf) {
            stf_elf_.open(elf);
            stf_elf_.readSymbols();
        }

        inline bool validPC(const uint64_t pc) const {
            return !!stf_elf_.findFunction(pc);
        }

        inline bool count(const uint64_t pc) {
            auto result = stf_elf_.findFunction(pc);
            if(result) {
                ++symbol_counts_[result.get()];
            }
            return !!result;
        }

        inline void dump() const {
            int max_symbol_length = 0;

            std::multimap<uint64_t, const STFElf::Symbol*> sorted_counts;

            for(const auto& symbol_pair: symbol_counts_) {
                const auto& symbol = *symbol_pair.first;
                const auto function_name_length = static_cast<int>(symbol.name().size() + (symbol.inlined() ? INLINE_SUFFIX.size() : 0));

                sorted_counts.emplace(symbol_pair.second, symbol_pair.first);
                max_symbol_length = std::max(max_symbol_length, function_name_length);
            }

            stf::print_utils::printLeft("Function", max_symbol_length);
            stf::print_utils::printSpaces(4);
            stf::print_utils::printLeft("Address", 16);
            stf::print_utils::printSpaces(4);
            stf::print_utils::printWidth("# Calls", 16);
            std::cout << std::endl;

            for(auto it = sorted_counts.rbegin(); it != sorted_counts.rend(); ++it) {
                const auto& symbol = *it->second;
                if(symbol.inlined()) {
                    stf::print_utils::printLeft(symbol.name() + std::string(INLINE_SUFFIX), max_symbol_length);
                }
                else {
                    stf::print_utils::printLeft(symbol.name(), max_symbol_length);
                }
                stf::print_utils::printSpaces(4);

                const auto& address_ranges = symbol.getRanges();
                auto range_it = address_ranges.begin();
                stf::print_utils::printHex(range_it->first);
                stf::print_utils::printSpaces(4);
                stf::print_utils::printDec(it->first, 16, ' ');
                std::cout << std::endl;

                for(++range_it; range_it != address_ranges.end(); ++range_it) {
                    stf::print_utils::printSpaces(static_cast<size_t>(max_symbol_length) + 4);
                    stf::print_utils::printHex(range_it->first);
                    std::cout << std::endl;
                }
            }
        }
};


