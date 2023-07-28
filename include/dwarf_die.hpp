#pragma once

#include "dwarf_attributes.hpp"

namespace dwarf_wrapper {
    class Die {
        private:
            const DwarfInterface* dwarf_ = nullptr;
            const Dwarf_Half tag_ = 0;
            Dwarf_Die die_ = nullptr;

            inline void nullify_() {
                dwarf_ = nullptr;
                die_ = nullptr;
            }

            inline Die makeDie_(const Dwarf_Die die) const {
                return Die(dwarf_, die);
            }

        public:
            Die() = default;

            Die(const DwarfInterface* dwarf, const Dwarf_Die die) :
                dwarf_(dwarf),
                tag_(die ? dwarf_->getTag(die) : 0),
                die_(die)
            {
            }

            Die(const DwarfInterface* dwarf, const Dwarf_Off offset) :
                Die(dwarf, dwarf->offDie(offset))
            {
            }

            Die(const Die&) = delete;

            Die(Die&& rhs) :
                dwarf_(rhs.dwarf_),
                tag_(rhs.tag_),
                die_(rhs.die_)
            {
                rhs.nullify_();
            }

            inline operator bool() const {
                return die_;
            }

            ~Die() {
                if(STF_EXPECT_TRUE(dwarf_ && die_)) {
                    dwarf_->deallocDie(die_);
                }
            }

            inline bool isInlinedSubroutine() const {
                return tag_ == DW_TAG_inlined_subroutine;
            }

            inline bool isSubprogram() const {
                return tag_ == DW_TAG_subprogram;
            }

            inline Die getSpecificationDie() const {
                if(const SpecificationAttribute spec_attr(dwarf_, die_); spec_attr) {
                    return Die(dwarf_, spec_attr.getReference());
                }
                return Die();
            }

            template<typename AttributeType>
            inline AttributeType getAttribute() const {
                const AttributeType result(dwarf_, die_);

                if(!result) {
                    if(const auto spec_die = getSpecificationDie()) {
                        return spec_die.getAttribute<AttributeType>();
                    }
                }

                return result;
            }

            inline bool isInlined() const {
                if(const auto inline_attr = getAttribute<InlineAttribute>()) {
                    return inline_attr.isInlined();
                }

                return false;
            }

            inline std::optional<uint64_t> getLowPC() const {
                std::optional<uint64_t> result = dwarf_->lowPc(die_);
                if(!result) {
                    if(const auto spec_die = getSpecificationDie()) {
                        return spec_die.getLowPC();
                    }
                }
                return result;
            }

            inline uint64_t getHighPC(const uint64_t low_pc) const {
                uint64_t high_pc;

                if(const auto result = dwarf_->highPc(die_); result.second != DW_FORM_CLASS_UNKNOWN) {
                    high_pc = result.first;
                    const auto form = result.second;

                    if (form != DW_FORM_CLASS_ADDRESS && !dwarf_addr_form_is_indexed(form)) {
                        high_pc += low_pc + (high_pc == 0);
                    }
                }
                else {
                    if(const auto spec_die = getSpecificationDie()) {
                        return spec_die.getHighPC(low_pc);
                    }
                    high_pc = low_pc + 1;
                }

                return high_pc;
            }

            inline std::optional<uint64_t> getInlineOffset() const {
                std::optional<uint64_t> result;

                if(const auto attr = getAttribute<AbstractOriginAttribute>()) {
                    result.emplace(attr.getInlineOffset());
                }

                return result;
            }

            inline const char* getLinkageName() const {
                if(const auto link_name_attr = getAttribute<LinkageNameAttribute>()) {
                    return link_name_attr.getName();
                }

                return nullptr;
            }

            inline const char* getName() const {
                const char* name = dwarf_->dieName(die_);

                if(!name) {
                    name = getLinkageName();
                    if(!name) {
                        if(const auto spec_die = getSpecificationDie()) {
                            name = spec_die.getName();
                        }
                    }
                }

                return name;
            }

            inline std::vector<STFAddressRange> getRanges() const {
                try {
                    if(const auto range_attr = getAttribute<RangeAttribute>()) {
                        return range_attr.getRanges(die_);
                    }
                }
                catch(const RangeAttribute::InvalidRangeAttribute&) {
                }

                return std::vector<STFAddressRange>();
            }

            inline uint64_t getOffset() const {
                return dwarf_->dieOffset(die_);
            }

            template<typename Callback>
            inline void iterateChildren(Callback&& callback,
                                        const Dwarf_Bool is_info,
                                        const int in_level = 0) const {
                if(const auto child = dwarf_->child(die_)) {
                    makeDie_(child).iterateSiblings(callback, is_info, in_level + 1);
                }
            }

            template<typename Callback>
            inline void iterateSiblings(Callback&& callback,
                                        const Dwarf_Bool is_info,
                                        const int in_level = 0) const {
                callback(*this);
                iterateChildren(callback, is_info, in_level);

                if(const auto sib_die = dwarf_->siblingOf(die_, is_info)) {
                    makeDie_(sib_die).iterateSiblings(callback, is_info, in_level);
                }
            }
    };
} // end namespace dwarf_wrapper
