#pragma once

#include <memory>
#include "dwarf_attributes.hpp"

namespace dwarf_wrapper {
    class Die : public std::enable_shared_from_this<Die> {
        private:
            struct enable_make_shared;

            const DwarfInterface* dwarf_ = nullptr;
            const Dwarf_Die die_ = nullptr;
            const Dwarf_Half tag_ = 0;

            inline std::shared_ptr<Die> makeDie_(const Dwarf_Die die) const {
                return construct(dwarf_, die);
            }

        protected:
            Die(const DwarfInterface* dwarf, const Dwarf_Die die) :
                dwarf_(dwarf),
                die_(die),
                tag_(die_ ? dwarf_->getTag(die_) : 0)
            {
            }

            Die(const DwarfInterface* dwarf, const Dwarf_Off offset) :
                Die(dwarf, dwarf->offDie(offset))
            {
            }

        public:
            Die(const Die&) = delete;

            Die(Die&& rhs) = delete;

            Die& operator=(const Die&) = delete;

            Die& operator=(Die&& rhs) = delete;

            template<typename ... Args>
            static inline std::shared_ptr<Die> construct(Args&&... args) {
                return std::make_shared<enable_make_shared>(std::forward<Args>(args)...);
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

            inline std::shared_ptr<Die> getSpecificationDie() const {
                if(const SpecificationAttribute spec_attr(dwarf_, die_); spec_attr) {
                    return makeDie_(spec_attr.getReference());
                }
                return nullptr;
            }

            template<typename AttributeType>
            inline AttributeType getAttribute() const {
                const AttributeType result(dwarf_, die_);

                if(!result) {
                    if(const auto spec_die = getSpecificationDie()) {
                        return spec_die->getAttribute<AttributeType>();
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
                        return spec_die->getLowPC();
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
                        return spec_die->getHighPC(low_pc);
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
                            name = spec_die->getName();
                        }
                    }
                }

                return name;
            }

            inline std::vector<STFAddressRange> getRanges() const {
                if(const auto range_attr = getAttribute<RangeAttribute>()) {
                    return range_attr.getRanges(die_);
                }

                return std::vector<STFAddressRange>();
            }

            inline uint64_t getOffset() const {
                return dwarf_->dieOffset(die_);
            }

            template<typename Callback>
            inline void iterateSiblings(Callback&& callback,
                                        const Dwarf_Bool is_info) {
                std::shared_ptr<Die> cur_die = shared_from_this();

                do {
                    callback(cur_die);

                    if(const auto child = dwarf_->child(cur_die->die_)) {
                        makeDie_(child)->iterateSiblings(callback, is_info);
                    }

                    if(const auto sib_die = dwarf_->siblingOf(cur_die->die_, is_info)) {
                        cur_die = makeDie_(sib_die);
                    }
                    else {
                        cur_die.reset();
                    }
                }
                while(cur_die);
            }
    };

    struct Die::enable_make_shared : public Die {
        template<typename ... Args>
        enable_make_shared(Args&&... args) :
            Die(std::forward<Args>(args)...)
        {
        }
    };
} // end namespace dwarf_wrapper
