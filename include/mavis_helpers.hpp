#pragma once

#include <limits>
#include <ostream>
#include "mavis/Mavis.h"
#include "mavis_json_files.hpp"
#include "stf_enum_utils.hpp"
#include "stf_exception.hpp"

namespace mavis_helpers {
    namespace __mavis_array {
        static constexpr size_t NUM_INSTRUCTION_TYPES_ = 33;

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
                IType::MASKABLE,
                IType::STRIDE,
                IType::INDEXED,
                IType::SEGMENT,
                IType::FAULTFIRST,
                IType::WHOLE,
                IType::MASK,
                IType::WIDENING,
                IType::HYPERVISOR,
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
                    case IType::MASKABLE:
                    case IType::STRIDE:
                    case IType::INDEXED:
                    case IType::SEGMENT:
                    case IType::FAULTFIRST:
                    case IType::WHOLE:
                    case IType::MASK:
                    case IType::CACHE:
                    case IType::ATOMIC:
                    case IType::FENCE:
                    case IType::SYSTEM:
                    case IType::CSR:
                    case IType::WIDENING:
                    case IType::HYPERVISOR:
                        break;
                };
            }

            // Finally, check that the last value in the array matches the expected end value
            // Otherwise the array is too large
            static_assert(arr.back() == END_VALUE, "NUM_INSTRUCTION_TYPES_ is too large");
            return arr;
        }

        static constexpr size_t NUM_ISA_EXTENSION_TYPES_ = 11;

        using ISAExtensionType = mavis::InstMetaData::ISAExtension;
        using ISAExtensionTypeArray = std::array<ISAExtensionType, NUM_ISA_EXTENSION_TYPES_>;

        static constexpr ISAExtensionTypeArray generateISAExtensionTypes_() {

            // Used to check the array size is correct
            constexpr auto END_VALUE = ISAExtensionType::V;

            // Manually initializing the array ensures that NUM_INSTRUCTION_TYPES_ is big enough
            // to fit all of the IType values
            constexpr ISAExtensionTypeArray arr = {
                ISAExtensionType::A,
                ISAExtensionType::B,
                ISAExtensionType::C,
                ISAExtensionType::D,
                ISAExtensionType::F,
                ISAExtensionType::G,
                ISAExtensionType::H,
                ISAExtensionType::I,
                ISAExtensionType::M,
                ISAExtensionType::Q,
                ISAExtensionType::V
            };

            // This looks silly, but it ensures that arr has every enumerated value in IType
            for(const auto i: arr) {
                switch(i) {
                    case ISAExtensionType::A:
                    case ISAExtensionType::B:
                    case ISAExtensionType::C:
                    case ISAExtensionType::D:
                    case ISAExtensionType::F:
                    case ISAExtensionType::G:
                    case ISAExtensionType::H:
                    case ISAExtensionType::I:
                    case ISAExtensionType::M:
                    case ISAExtensionType::Q:
                    case ISAExtensionType::V:
                        break;
                };
            }

            // Finally, check that the last value in the array matches the expected end value
            // Otherwise the array is too large
            static_assert(arr.back() == END_VALUE, "NUM_ISA_EXTENSION_TYPES_ is too large");
            return arr;
        }

    } // end namespace __mavis_array

    class MavisInstTypeArray {
        private:
            static constexpr __mavis_array::InstructionTypeArray instruction_type_array_ = __mavis_array::generateInstructionTypes_();

        public:
            using enum_t = __mavis_array::IType;
            using int_t = stf::enums::int_t<enum_t>;

            static constexpr enum_t UNDEFINED = static_cast<enum_t>(0);

            static constexpr auto begin() {
                return instruction_type_array_.begin();
            }

            static constexpr auto end() {
                return instruction_type_array_.end();
            }

            static const char* getTypeString(const enum_t type) {
                if(STF_EXPECT_FALSE(type == UNDEFINED)) {
                    return "undef";
                }

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
                    case enum_t::MASKABLE:
                        return "maskable";
                    case enum_t::STRIDE:
                        return "stride";
                    case enum_t::INDEXED:
                        return "indexed";
                    case enum_t::SEGMENT:
                        return "segment";
                    case enum_t::FAULTFIRST:
                        return "faultfirst";
                    case enum_t::WHOLE:
                        return "whole";
                    case enum_t::MASK:
                        return "mask";
                    case enum_t::WIDENING:
                        return "widening";
                    case enum_t::HYPERVISOR:
                        return "hypervisor";
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

    class MavisISAExtensionTypeArray {
        private:
            static constexpr __mavis_array::ISAExtensionTypeArray isa_extension_type_array_ = __mavis_array::generateISAExtensionTypes_();

        public:
            using enum_t = __mavis_array::ISAExtensionType;
            using int_t = stf::enums::int_t<enum_t>;

            static constexpr auto begin() {
                return isa_extension_type_array_.begin();
            }

            static constexpr auto end() {
                return isa_extension_type_array_.end();
            }

            static const char* getTypeString(const enum_t type) {
                switch(type) {
                    case enum_t::A:
                        return "A";
                    case enum_t::B:
                        return "B";
                    case enum_t::C:
                        return "C";
                    case enum_t::D:
                        return "D";
                    case enum_t::F:
                        return "F";
                    case enum_t::G:
                        return "G";
                    case enum_t::H:
                        return "H";
                    case enum_t::I:
                        return "I";
                    case enum_t::M:
                        return "M";
                    case enum_t::Q:
                        return "Q";
                    case enum_t::V:
                        return "V";
                };

                stf_throw("Invalid ISA extension type specified: " << stf::enums::to_printable_int(type));
            }
    };

    /**
     * \class InstType
     * \brief Stub class used for instantiating Mavis instance
     */
    class InstType {
        public:
            using PtrType = std::shared_ptr<InstType>;
    };

    /**
     * \class DummyAnnotationType
     * \brief Stub class used for instantiating Mavis instance
     */
    class DummyAnnotationType {
        public:
            using PtrType = std::shared_ptr<DummyAnnotationType>;

            DummyAnnotationType() = default;
            DummyAnnotationType(const DummyAnnotationType&) = default;
            explicit DummyAnnotationType(const nlohmann::json& inst) {}
            void update(const nlohmann::json& inst) const {}

            /**
             * Writes an DummyAnnotationType out to an std::ostream. Just a stub for now.
             * \param os std::ostream to use
             * \param anno DummyAnnotationType to write
             */
            friend inline std::ostream& operator<<(std::ostream& os, const DummyAnnotationType& anno) {
                (void)anno;
                return os;
            }
    };

    /**
     * \class AnnotationType
     * \brief Stub class used for instantiating Mavis instance
     */
    class AnnotationType : public DummyAnnotationType {
        private:
            const mavis::FormWrapperIF* form_;
            inline static std::unordered_map<std::string, std::string> mnemonic_map_;

        public:
            using PtrType = std::shared_ptr<AnnotationType>;

            AnnotationType() = default;
            AnnotationType(const AnnotationType&) = default;
            explicit AnnotationType(const nlohmann::json& inst) {
                update(inst);
            }

            inline void update(const nlohmann::json& inst) {
                auto mnemonic_it = inst.find("mnemonic");
                stf_assert(mnemonic_it != inst.end(), "Failed to find mnemonic for instruction: " << inst.dump());

                auto form_it = inst.find("form");

                if(form_it == inst.end()) {
                    auto overlay_it = inst.find("overlay");
                    stf_assert(overlay_it != inst.end(), "Failed to find overlay for form-less instruction: " << inst.dump());
                    auto parent_mnemonic_it = overlay_it->find("base");
                    stf_assert(parent_mnemonic_it != overlay_it->end(), "Failed to find base in overlay for instruction: " << inst.dump());
                    auto parent_form_it = mnemonic_map_.find(*parent_mnemonic_it);
                    stf_assert(parent_form_it != mnemonic_map_.end(), "Failed to find mnemonic in map for instruction: " << inst.dump());
                    form_ = mavis::FormRegistry::getFormWrapper(parent_form_it->second);
                    mnemonic_map_.try_emplace(*mnemonic_it, parent_form_it->second);
                }
                else {
                    form_ = mavis::FormRegistry::getFormWrapper(*form_it);
                    mnemonic_map_.try_emplace(*mnemonic_it, *form_it);
                }
            }

            inline const auto& getOpcodeFields() const {
                return form_->getOpcodeFields();
            }

            inline const auto& getField(const std::string& name) const {
                return form_->getField(name);
            }
    };

    /**
     * \typedef Mavis
     * \brief Mavis decoder type
     */
    using Mavis = ::Mavis<InstType, DummyAnnotationType>;

    /**
     * \typedef FullMavis
     * \brief Mavis decoder type that includes annotations
     */
    using FullMavis = ::Mavis<InstType, AnnotationType>;
} // end namespace mavis_helpers
