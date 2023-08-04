#pragma once

#include <cstring>
#include <optional>
#include <string>
#include <sstream>
#include <vector>

#include "dwarf_die.hpp"

class STFDwarf {
    private:
        const dwarf_wrapper::DwarfInterface dwarf_;

    public:
        explicit STFDwarf(const std::string& filename) :
            dwarf_(filename)
        {
        }

        template<typename Callback>
        inline void iterateDies(Callback&& callback) {
            static constexpr Dwarf_Bool is_info = dwarf_wrapper::DWARF_TRUE;
            static constexpr Dwarf_Die no_die = nullptr;

            while(dwarf_.nextCuHeader(is_info)) {
                /* The CU will have a single sibling, a cu_die. */
                const auto cu_die = dwarf_.siblingOf(no_die, is_info);
                stf_assert(cu_die, "Error reading CU siblings");
                dwarf_wrapper::Die::construct(&dwarf_, cu_die)->iterateSiblings(callback, is_info);
            }
        }
};
