#pragma once

#include <vector>

#include "libdwarf_wrapper.hpp"
#include "stf_address_range.hpp"

namespace dwarf_wrapper {
    class Attribute {
        protected:
            const DwarfInterface* dwarf_ = nullptr;
            const Dwarf_Attribute attr_ = nullptr;

            inline Dwarf_Signed formSData_() const {
                return dwarf_->formSData(attr_);
            }

            inline const char* formString_() const {
                return dwarf_->formString(attr_);
            }

            inline Dwarf_Unsigned formUData_() const {
                return dwarf_->formUData(attr_);
            }

            inline Dwarf_Off globalFormRef_() const {
                return dwarf_->globalFormRef(attr_);
            }

            inline Dwarf_Half getForm_() const {
                return dwarf_->whatForm(attr_);
            }

        public:
            Attribute(const DwarfInterface* dwarf, const Dwarf_Attribute attr) :
                dwarf_(dwarf),
                attr_(attr)
            {
            }

            Attribute(const DwarfInterface* dwarf, const Dwarf_Die die, const Dwarf_Half attr) :
                Attribute(dwarf, dwarf->getAttribute(die, attr))
            {
            }

            inline operator bool() const {
                return attr_;
            }
    };

    class FormAttribute : public Attribute {
        protected:
            const Dwarf_Half form_;

        public:
            FormAttribute(const DwarfInterface* dwarf, const Dwarf_Die die, const Dwarf_Half attr) :
                Attribute(dwarf, die, attr),
                form_(*this ? getForm_() : 0)
            {
            }
    };

    class SpecificationAttribute : public FormAttribute {
        public:
            SpecificationAttribute(const DwarfInterface* dwarf, const Dwarf_Die die) :
                FormAttribute(dwarf, die, DW_AT_specification)
            {
            }

            inline Dwarf_Die getReference() const {
                switch(form_) {
                    case DW_FORM_ref_addr:
                    case DW_FORM_ref_sup4: /* DWARF5 */
                    case DW_FORM_ref_sup8: /* DWARF5 */
                    case DW_FORM_GNU_ref_alt:
                        return dwarf_->offDie(globalFormRef_());

                    case DW_FORM_ref1:
                    case DW_FORM_ref2:
                    case DW_FORM_ref4:
                    case DW_FORM_ref8:
                    case DW_FORM_ref_udata:
                        return dwarf_->offDie(dwarf_->convertToGlobalOffset(attr_,
                                                                            dwarf_->formRef(attr_)));
                    case DW_FORM_ref_sig8: /* DWARF4 */
                        return dwarf_->findDieFromSig8(dwarf_->formSig8(attr_));
                    default:
                        stf_throw("Unexpected DW_AT_specification form: " << form_);
                }
            }
    };

    class InlineAttribute : public Attribute {
        public:
            InlineAttribute(const DwarfInterface* dwarf, const Dwarf_Die die) :
                Attribute(dwarf, die, DW_AT_inline)
            {
            }

            inline bool isInlined() const {
                const Dwarf_Signed inlined_state = formSData_();

                return inlined_state == DW_INL_inlined || inlined_state == DW_INL_declared_inlined;
            }
    };

    class AbstractOriginAttribute : public Attribute {
        public:
            AbstractOriginAttribute(const DwarfInterface* dwarf, const Dwarf_Die die) :
                Attribute(dwarf, die, DW_AT_abstract_origin)
            {
            }

            inline uint64_t getInlineOffset() const {
                return globalFormRef_();
            }
    };

    class LinkageNameAttribute : public Attribute {
        public:
            LinkageNameAttribute(const DwarfInterface* dwarf, const Dwarf_Die die) :
                Attribute(dwarf, die, DW_AT_linkage_name)
            {
            }

            inline const char* getName() const {
                return formString_();
            }
    };

    class RangeAttribute : public FormAttribute {
        private:
            const Dwarf_Unsigned offset_;

            template<typename RangeList, typename ... Args>
            static inline std::vector<STFAddressRange> populateRanges_(Args&&... range_list_args) {
                const auto range_list = RangeList(std::forward<Args>(range_list_args)...);
                return std::vector<STFAddressRange>(range_list.begin(), range_list.end());
            }

            struct RangeEntry {
                Dwarf_Addr start_address = 0;
                Dwarf_Addr end_address = 0;

                inline operator STFAddressRange() const {
                    return STFAddressRange(start_address, end_address);
                }
            };

            template<typename Pointer,
                     typename Size,
                     typename IteratorPointer = Pointer,
                     typename IteratorValue = std::remove_pointer_t<IteratorPointer>>
            class RangeListBase {
                public:
                    class base_iterator {
                        protected:
                            IteratorPointer ptr_;

                        public:
                            using difference_type = std::ptrdiff_t;
                            using value_type = IteratorValue;
                            using pointer = IteratorValue*;
                            using reference = IteratorValue&;
                            using const_reference = const IteratorValue&;
                            using iterator_category = std::forward_iterator_tag;

                            explicit base_iterator(const IteratorPointer ptr) :
                                ptr_(ptr)
                            {
                            }

                            inline base_iterator& operator++() {
                                ++ptr_;
                                return *this;
                            }

                            inline base_iterator operator++(int) {
                                const auto temp = *this;
                                ++(*this);
                                return temp;
                            }

                            inline bool operator==(const base_iterator& rhs) const {
                                return ptr_ == rhs.ptr_;
                            }

                            inline bool operator!=(const base_iterator& rhs) const {
                                return !(*this == rhs);
                            }

                            inline const_reference operator*() const {
                                return *ptr_;
                            }

                            inline const pointer operator->() const {
                                return ptr_;
                            }

                            inline difference_type operator-(const base_iterator& rhs) const {
                                return ptr_ - rhs.ptr_;
                            }
                    };

                    class base_entry_iterator : public base_iterator {
                        protected:
                            RangeEntry entry_;

                            virtual void postIncrementUpdate_() {
                            }

                        public:
                            using value_type = RangeEntry;
                            using pointer = RangeEntry*;
                            using reference = RangeEntry&;

                            explicit base_entry_iterator(IteratorPointer ptr) :
                                base_iterator(ptr)
                            {
                            }

                            inline const RangeEntry& operator*() const {
                                return entry_;
                            }

                            inline const RangeEntry* operator->() const {
                                return &entry_;
                            }

                            inline base_entry_iterator& operator++() {
                                base_iterator::operator++();
                                postIncrementUpdate_();

                                return *this;
                            }

                            inline base_entry_iterator operator++(int) {
                                auto temp = *this;
                                ++(*this);
                                return temp;
                            }

                            using base_iterator::operator==;
                            using base_iterator::operator!=;
                    };

                protected:
                    const DwarfInterface* dwarf_ = nullptr;
                    Pointer head_ = nullptr;
                    Size size_ = 0;

                    inline void nullify_() {
                        dwarf_ = nullptr;
                        head_ = nullptr;
                        size_ = 0;
                    }

                public:
                    RangeListBase(const DwarfInterface* const dwarf,
                                  const Pointer head,
                                  const Size size) :
                        dwarf_(dwarf),
                        head_(head),
                        size_(size)
                    {
                    }

                    RangeListBase(const RangeListBase&) = delete;
                    RangeListBase(RangeListBase&& rhs) :
                        dwarf_(rhs.dwarf_),
                        head_(rhs.head_),
                        size_(rhs.size_)
                    {
                        rhs.nullify_();
                    }

                    RangeListBase& operator=(const RangeListBase&) = delete;

                    inline RangeListBase& operator=(RangeListBase&& rhs) {
                        dwarf_ = rhs.dwarf_;
                        head_ = rhs.head_;
                        size_ = rhs.size_;
                        rhs.nullify_();
                        return *this;
                    }

                    inline size_t size() const {
                        return static_cast<size_t>(size_);
                    }
            };

            class RangeListDW4 final : public RangeListBase<Dwarf_Ranges*, Dwarf_Signed> {
                private:
                    RangeListDW4(const DwarfInterface* const dwarf,
                                 std::pair<Dwarf_Ranges*, Dwarf_Signed>&& range_info) :
                        RangeListDW4(dwarf, range_info.first, range_info.second)
                    {
                        if(STF_EXPECT_FALSE(!head_)) {
                            throw InvalidRangeAttribute();
                        }
                    }

                public:
                    using iterator = RangeListBase::base_iterator;

                    class addr_iterator : public RangeListBase::base_entry_iterator {
                        private:
                            Dwarf_Addr base_addr_ = 0;

                            inline bool updateEntry_() {
                                switch(ptr_->dwr_type) {
                                    case DW_RANGES_ENTRY:
                                        if(STF_EXPECT_FALSE(ptr_->dwr_addr1 == ptr_->dwr_addr2)) {
                                            return true;
                                        }

                                        entry_.start_address = ptr_->dwr_addr1 + base_addr_;
                                        entry_.end_address = ptr_->dwr_addr2 + base_addr_;
                                        break;

                                    case DW_RANGES_ADDRESS_SELECTION:
                                        base_addr_ = ptr_->dwr_addr2;
                                        return true;

                                    case DW_RANGES_END:
                                        // We reached the end
                                        // increment the pointer one more time so we point to the end iterator,
                                        // but don't indicate that we should keep looping
                                        iterator::operator++();
                                        break;
                                }
                                return false;
                            }

                            void postIncrementUpdate_() final {
                                if(updateEntry_()) {
                                    ++(*this);
                                }
                            }

                        public:
                            explicit addr_iterator(Dwarf_Ranges* const ptr, const bool end = false) :
                                base_entry_iterator(ptr)
                            {
                                if(!end) {
                                    postIncrementUpdate_();
                                }
                            }
                    };

                    RangeListDW4(const DwarfInterface* dwarf,
                                 Dwarf_Ranges* const head,
                                 const Dwarf_Signed size) :
                        RangeListBase(dwarf, head, size)
                    {
                    }

                    RangeListDW4(const DwarfInterface* dwarf,
                                 const Dwarf_Die die,
                                 const Dwarf_Off offset) :
                        RangeListDW4(dwarf, dwarf->getDwarfRanges(die, offset))
                    {
                    }

                    RangeListDW4(RangeListDW4&&) = default;
                    RangeListDW4& operator=(RangeListDW4&&) = default;

                    ~RangeListDW4() {
                        if(STF_EXPECT_TRUE(dwarf_)) {
                            dwarf_->deallocRanges(head_, size_);
                        }
                    }

                    inline iterator beginRaw() const {
                        return iterator(head_);
                    }

                    inline iterator endRaw() const {
                        return iterator(head_ + size_);
                    }

                    inline addr_iterator begin() const {
                        return addr_iterator(head_);
                    }

                    inline addr_iterator end() const {
                        return addr_iterator(head_ + size_, true);
                    }
            };

            class RangeListDW5 final : public RangeListBase<Dwarf_Rnglists_Head,
                                                            Dwarf_Unsigned,
                                                            Dwarf_Unsigned,
                                                            RangeEntry> {
                private:
                    inline void getNextEntry_(const Dwarf_Unsigned index,
                                              unsigned& rle_code,
                                              RangeEntry& entry) const {
                        if(STF_EXPECT_FALSE(!dwarf_->getRangeListEntry(head_,
                                                                       index,
                                                                       rle_code,
                                                                       entry.start_address,
                                                                       entry.end_address))) {
                            throw InvalidRangeAttribute();
                        }
                    }

                    RangeListDW5(const DwarfInterface* dwarf,
                                 std::pair<Dwarf_Rnglists_Head, Dwarf_Unsigned>&& range_info) :
                        RangeListDW5(dwarf, range_info.first, range_info.second)
                    {
                        if(STF_EXPECT_FALSE(!head_)) {
                            throw InvalidRangeAttribute();
                        }
                    }

                public:
                    class iterator : public RangeListBase::base_entry_iterator {
                        protected:
                            const RangeListDW5* parent_;
                            unsigned rle_code_ = 0;

                            void postIncrementUpdate_() override {
                                if(STF_EXPECT_TRUE(ptr_ < parent_->size())) {
                                    parent_->getNextEntry_(ptr_, rle_code_, entry_);
                                    if(STF_EXPECT_FALSE(rle_code_ == DW_RLE_end_of_list)) {
                                        ptr_ = parent_->size();
                                    }
                                }
                            }

                        public:
                            explicit iterator(const RangeListDW5* parent) :
                                RangeListBase::base_entry_iterator(parent->size()),
                                parent_(parent)
                            {
                            }

                            iterator(const RangeListDW5* parent, const Dwarf_Unsigned index) :
                                RangeListBase::base_entry_iterator(index),
                                parent_(parent)
                            {
                                iterator::postIncrementUpdate_();
                            }

                            inline bool operator==(const iterator& rhs) const {
                                return (parent_ == rhs.parent_) &&
                                       RangeListBase::base_entry_iterator::operator==(rhs);
                            }

                            inline bool operator!=(const iterator& rhs) const {
                                return !(*this == rhs);
                            }
                    };

                    class addr_iterator : public iterator {
                        private:
                            inline bool isAddressEntry_() const {
                                switch(rle_code_) {
                                    case DW_RLE_startx_endx:
                                    case DW_RLE_startx_length:
                                    case DW_RLE_offset_pair:
                                    case DW_RLE_start_end:
                                    case DW_RLE_start_length:
                                        return true;
                                    default:
                                        return false;
                                }
                            }

                            inline bool skipEntry_() const {
                                return ptr_ != parent_->size() && 
                                       (!isAddressEntry_() ||
                                        (entry_.start_address == entry_.end_address));
                            }

                            inline void advanceIfSkipped_() {
                                if(skipEntry_()) {
                                    ++(*this);
                                }
                            }

                            void postIncrementUpdate_() final {
                                iterator::postIncrementUpdate_();
                                advanceIfSkipped_();
                            }

                        public:
                            explicit addr_iterator(const RangeListDW5* parent) :
                                iterator(parent)
                            {
                            }

                            addr_iterator(const RangeListDW5* parent, const Dwarf_Unsigned index) :
                                iterator(parent, index)
                            {
                                advanceIfSkipped_();
                            }
                    };

                    RangeListDW5(const DwarfInterface* dwarf,
                                 const Dwarf_Rnglists_Head head,
                                 const Dwarf_Unsigned size) :
                        RangeListBase(dwarf, head, size)
                    {
                    }

                    RangeListDW5(const DwarfInterface* dwarf,
                                 const Dwarf_Attribute attr,
                                 const Dwarf_Half form,
                                 const Dwarf_Off offset) :
                        RangeListDW5(dwarf, dwarf->getDwarfRngLists(attr, form, offset))
                    {
                    }

                    RangeListDW5(RangeListDW5&&) = default;

                    RangeListDW5& operator=(RangeListDW5&&) = default;

                    ~RangeListDW5() {
                        if(head_) {
                            dwarf_dealloc_rnglists_head(head_);
                        }
                    }

                    inline iterator beginRaw() const {
                        return iterator(this, 0);
                    }

                    inline iterator endRaw() const {
                        return iterator(this);
                    }

                    inline addr_iterator begin() const {
                        return addr_iterator(this, 0);
                    }

                    inline addr_iterator end() const {
                        return addr_iterator(this);
                    }
            };

            inline Dwarf_Off getOffset_() const {
                try {
                    if(form_ == DW_FORM_rnglistx) {
                        return formUData_();
                    }

                    return globalFormRef_();
                }
                catch(const dwarf_wrapper::DwarfError&) {
                    if(STF_EXPECT_FALSE(form_ == DW_FORM_rnglistx)) {
                        throw;
                    }
                    throw InvalidRangeAttribute();
                }
            }

        public:
            class InvalidRangeAttribute : public std::exception {
            };

            RangeAttribute(const DwarfInterface* dwarf, const Dwarf_Die die) :
                FormAttribute(dwarf, die, DW_AT_ranges),
                offset_(getOffset_())
            {
            }

            inline std::vector<STFAddressRange> getRanges(const Dwarf_Die die) const {
                static constexpr Dwarf_Half DWVERSION5 = 5;

                const auto cu_version = dwarf_->getDieVersion(die);

                if(STF_EXPECT_FALSE(!cu_version)) {
                    throw InvalidRangeAttribute();
                }

                if(cu_version < DWVERSION5) {
                    return populateRanges_<RangeListDW4>(dwarf_, die, offset_);
                }

                return populateRanges_<RangeListDW5>(dwarf_, attr_, form_, offset_);
            }
    };
} // end namespace dwarf_wrapper
