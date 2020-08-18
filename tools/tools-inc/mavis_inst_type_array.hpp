#pragma once

#include <limits>
#include <ostream>
#include "stf_enum_utils.hpp"
#include "stf_exception.hpp"
#include "mavis/Mavis.h"

namespace __mavis_array {
    static constexpr size_t NUM_INSTRUCTION_TYPES_ = 27;

    using IType = mavis::InstMetaData::InstructionTypes;
    using InstructionTypeArray = std::array<IType, NUM_INSTRUCTION_TYPES_>;
    static constexpr InstructionTypeArray generateInstructionTypes_() {

        // Used to check the array size is correct
        constexpr auto END_VALUE = IType::CSR;

        // Manually initializing the array ensures that NUM_INSTRUCTION_TYPES_ is big enough
        // to fit all of the IType values
        constexpr InstructionTypeArray arr = {
            IType::INT,
            IType::FLOAT,
            IType::ARITH,
            IType::MULTIPLY,
            IType::DIVIDE,
            IType::BRANCH,
            IType::PC,
            IType::CONDITIONAL,
            IType::JAL,
            IType::JALR,
            IType::LOAD,
            IType::STORE,
            IType::MAC,
            IType::SQRT,
            IType::CONVERT,
            IType::COMPARE,
            IType::MOVE,
            IType::CLASSIFY,
            IType::VECTOR,
            IType::MASK,
            IType::INDEXED,
            IType::SEGMENT,
            IType::CACHE,
            IType::ATOMIC,
            IType::FENCE,
            IType::SYSTEM,
            IType::CSR
        };

        // This looks silly, but it ensures that arr has every enumerated value in IType
        for(const auto i: arr) {
            switch(i) {
                case IType::INT:
                case IType::FLOAT:
                case IType::ARITH:
                case IType::MULTIPLY:
                case IType::DIVIDE:
                case IType::BRANCH:
                case IType::PC:
                case IType::CONDITIONAL:
                case IType::JAL:
                case IType::JALR:
                case IType::LOAD:
                case IType::STORE:
                case IType::MAC:
                case IType::SQRT:
                case IType::CONVERT:
                case IType::COMPARE:
                case IType::MOVE:
                case IType::CLASSIFY:
                case IType::VECTOR:
                case IType::MASK:
                case IType::INDEXED:
                case IType::SEGMENT:
                case IType::CACHE:
                case IType::ATOMIC:
                case IType::FENCE:
                case IType::SYSTEM:
                case IType::CSR:
                    break;
            };
        }

        // Finally, check that the last value in the array matches the expected end value
        // Otherwise the array is too large
        static_assert(arr.back() == END_VALUE, "NUM_INSTRUCTION_TYPES_ is too large");
        return arr;
    }
} // end namespace __mavis_array

class MavisInstTypeArray {
    private:
        static constexpr __mavis_array::InstructionTypeArray instruction_type_array_ = __mavis_array::generateInstructionTypes_();

    public:
        using enum_t = __mavis_array::IType;
        using int_t = stf::enums::int_t<enum_t>;

        static constexpr auto begin() {
            return instruction_type_array_.begin();
        }

        static constexpr auto end() {
            return instruction_type_array_.end();
        }

        static const char* getTypeString(const enum_t type) {
            switch(type) {
                case enum_t::INT:
                    return "int";
                case enum_t::FLOAT:
                    return "float";
                case enum_t::ARITH:
                    return "arith";
                case enum_t::MULTIPLY:
                    return "mul";
                case enum_t::DIVIDE:
                    return "div";
                case enum_t::BRANCH:
                    return "branch";
                case enum_t::PC:
                    return "pc";
                case enum_t::CONDITIONAL:
                    return "cond";
                case enum_t::JAL:
                    return "jal";
                case enum_t::JALR:
                    return "jalr";
                case enum_t::LOAD:
                    return "load";
                case enum_t::STORE:
                    return "store";
                case enum_t::MAC:
                    return "mac";
                case enum_t::SQRT:
                    return "sqrt";
                case enum_t::CONVERT:
                    return "convert";
                case enum_t::COMPARE:
                    return "compare";
                case enum_t::MOVE:
                    return "move";
                case enum_t::CLASSIFY:
                    return "classify";
                case enum_t::VECTOR:
                    return "vector";
                case enum_t::MASK:
                    return "mask";
                case enum_t::INDEXED:
                    return "indexed";
                case enum_t::SEGMENT:
                    return "segment";
                case enum_t::CACHE:
                    return "cache";
                case enum_t::ATOMIC:
                    return "atomic";
                case enum_t::FENCE:
                    return "fence";
                case enum_t::SYSTEM:
                    return "system";
                case enum_t::CSR:
                    return "csr";
            };

            stf_throw("Invalid instruction type specified: " << stf::enums::to_printable_int(type));
        }
};
