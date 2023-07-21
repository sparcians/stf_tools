#pragma once

#include <cstring>
#include <optional>
#include <string>
#include <sstream>
#include <unordered_map>

#include <boost/core/demangle.hpp>

#include "stf_address_range.hpp"

#include "dwarf.h"
#include "libdwarf.h"

#define _DWARF_STR(s) #s
#define DWARF_STR(s) _DWARF_STR(s)
#define DWARF_ERR_MSG(func) "Error calling libdwarf function " #func " at " __FILE__ ":" DWARF_STR(__LINE__)

#define DWARF_CALL(func, ...) dwarfCall_(DWARF_ERR_MSG(func), func, __VA_ARGS__)
#define DWARF_CALL_NO_ERR(func, ...) dwarfCallNoErr_(DWARF_ERR_MSG(func), func, __VA_ARGS__)
#define DWARF_CALL_ASSERT_OK(func, ...) dwarfCallAssertOk_(DWARF_ERR_MSG(func), func, __VA_ARGS__)

#define DIE_DWARF_CALL(...) dwarf_->DWARF_CALL(__VA_ARGS__)
#define DIE_DWARF_CALL_NO_ERR(...) dwarf_->DWARF_CALL_NO_ERR(__VA_ARGS__)
#define DIE_DWARF_CALL_ASSERT_OK(...) dwarf_->DWARF_CALL_ASSERT_OK(__VA_ARGS__)

class STFDwarf {
    public:
        class NoDwarfInfoException : public std::exception {
        };

    private:
        class DwarfError : public std::exception {
            private:
                std::string msg_;

            public:
                explicit DwarfError(const std::string& msg) :
                    msg_(msg)
                {
                }

                const char* what() const noexcept override {
                    return msg_.c_str();
                }
        };

        inline static constexpr Dwarf_Bool DWARF_TRUE_ = 1;
        inline static constexpr Dwarf_Bool DWARF_FALSE_ = 0;

        const Dwarf_Debug dbg_ = 0;

        mutable Dwarf_Error err_ = 0;
        mutable int res_ = 0;

        inline static Dwarf_Debug init_(const std::string& filename) {
            Dwarf_Debug dbg = 0;

            char pathbuf[FILENAME_MAX];

            Dwarf_Error err = 0;
            const int res = dwarf_init_path(filename.c_str(),
                                            pathbuf,
                                            sizeof(pathbuf),
                                            DW_GROUPNUMBER_ANY,
                                            nullptr,
                                            nullptr,
                                            &dbg,
                                            &err);

            if(STF_EXPECT_FALSE(res == DW_DLV_ERROR)) {
                dwarf_dealloc_error(dbg, err);
                stf_throw("Failed to open " << filename << " with DWARF parser");
            }

            if(STF_EXPECT_FALSE(res == DW_DLV_NO_ENTRY)) {
                throw NoDwarfInfoException();
            }

            return dbg;
        }

        template<typename Func, typename ... Args>
        inline void dwarfCallNoErr_(const char* dwarf_err_msg,
                                    Func&& func,
                                    Args&&... args) const {
            res_ = func(std::forward<Args>(args)...);
            if(STF_EXPECT_FALSE(isError_())) {
                throw DwarfError(dwarf_err_msg);
            }
        }

        template<typename Func, typename ... Args>
        inline void dwarfCallAssertOk_(const char* dwarf_err_msg,
                               Func&& func,
                               Args&&... args) const {
            res_ = func(std::forward<Args>(args)..., &err_);
            if(STF_EXPECT_FALSE(!isOk_())) {
                throw DwarfError(dwarf_err_msg);
            }
        }

        template<typename Func, typename ... Args>
        inline void dwarfCall_(const char* dwarf_err_msg,
                               Func&& func,
                               Args&&... args) const {
            dwarfCallNoErr_(dwarf_err_msg,
                            std::forward<Func>(func),
                            std::forward<Args>(args)...,
                            &err_);
        }

        inline bool isOk_() const {
            return res_ == DW_DLV_OK;
        }

        inline bool isError_() const {
            return res_ == DW_DLV_ERROR;
        }

        inline bool isNoEntry_() const {
            return res_ == DW_DLV_NO_ENTRY;
        }

        template<typename ... Args>
        inline std::string genErrMsg_(Args&&... args) const {
            std::ostringstream ss;
            (ss << ... << args) << ": " << dwarf_errmsg(err_);
            stf_throw(ss.str());
        }

        inline void deallocDie_(const Dwarf_Die die) const {
            if(STF_EXPECT_TRUE(die)) {
                dwarf_dealloc(dbg_, die, DW_DLA_DIE);
            }
        }

    private:
        class Ranges {
            private:
                const STFDwarf* dwarf_ = nullptr;
                Dwarf_Ranges* rangeset_ = nullptr;
                Dwarf_Signed rangecount_ = 0;

                inline void nullify_() {
                    dwarf_ = nullptr;
                    rangeset_ = nullptr;
                }

            public:
                class iterator {
                    private:
                        Dwarf_Ranges* ptr_;

                    public:
                        explicit iterator(Dwarf_Ranges* ptr) :
                            ptr_(ptr)
                        {
                        }

                        inline iterator& operator++() {
                            ++ptr_;
                            return *this;
                        }

                        inline iterator operator++(int) {
                            const auto temp = *this;
                            ++(*this);
                            return temp;
                        }

                        inline bool operator==(const iterator& rhs) const {
                            return ptr_ == rhs.ptr_;
                        }

                        inline bool operator!=(const iterator& rhs) const {
                            return !(*this == rhs);
                        }

                        inline const Dwarf_Ranges& operator*() const {
                            return *ptr_;
                        }

                        inline const Dwarf_Ranges* operator->() const {
                            return ptr_;
                        }
                };

                Ranges(const STFDwarf* dwarf, Dwarf_Ranges* rangeset, Dwarf_Signed rangecount) :
                    dwarf_(dwarf),
                    rangeset_(rangeset),
                    rangecount_(rangecount)
                {
                }

                ~Ranges() {
                    if(STF_EXPECT_TRUE(dwarf_)) {
                        dwarf_->deallocRanges_(rangeset_, rangecount_);
                    }
                }

                Ranges(const Ranges&) = delete;
                Ranges(Ranges&& rhs) :
                    dwarf_(rhs.dwarf_),
                    rangeset_(rhs.rangeset_),
                    rangecount_(rhs.rangecount_)
                {
                    rhs.nullify_();
                }

                Ranges& operator=(const Ranges&) = delete;
                Ranges& operator=(Ranges&& rhs)
                {
                    dwarf_ = rhs.dwarf_;
                    rangeset_ = rhs.rangeset_;
                    rangecount_ = rhs.rangecount_;
                    rhs.nullify_();
                    return *this;
                }

                iterator begin() const {
                    return iterator(rangeset_);
                }

                iterator end() const {
                    return iterator(rangeset_ + rangecount_);
                }
        };

        inline std::optional<Ranges> getDwarfRanges_(const Dwarf_Die die,
                                                     const Dwarf_Unsigned original_off) const {
            std::optional<Ranges> result;
            Dwarf_Ranges *rangeset = 0;
            Dwarf_Signed rangecount = 0;
            DWARF_CALL(dwarf_get_ranges_b,
                       dbg_,
                       original_off,
                       die,
                       nullptr,
                       &rangeset,
                       &rangecount,
                       nullptr);

            if(isOk_()) {
                result.emplace(this, rangeset, rangecount);
            }

            return result;
        }

        inline void deallocRanges_(Dwarf_Ranges* rangeset, const Dwarf_Signed rangecount) const {
            if(STF_EXPECT_TRUE(rangeset)) {
                dwarf_dealloc_ranges(dbg_, rangeset, rangecount);
            }
        }

    public:
        class Die {
            private:
                enum class NameState {
                    UNKNOWN,
                    HAS_NAME,
                    NO_NAME
                };

                const STFDwarf* dwarf_ = nullptr;
                Dwarf_Die die_ = 0;
                mutable Dwarf_Half tag_ = 0;

                inline void nullify_() {
                    dwarf_ = nullptr;
                    die_ = 0;
                }

                Die getReference_(const Dwarf_Attribute attr) const {
                    Dwarf_Half form;
                    DIE_DWARF_CALL_ASSERT_OK(dwarf_whatform,
                                             attr,
                                             &form);
                    switch(form) {
                        case DW_FORM_ref_addr:
                        case DW_FORM_ref_sup4: /* DWARF5 */
                        case DW_FORM_ref_sup8: /* DWARF5 */
                        case DW_FORM_GNU_ref_alt:
                            {
                                Dwarf_Off offset;
                                DIE_DWARF_CALL_ASSERT_OK(dwarf_global_formref,
                                                         attr,
                                                         &offset);
                                return dwarf_->getDieFromOffset(offset);
                            }
                        case DW_FORM_ref1:
                        case DW_FORM_ref2:
                        case DW_FORM_ref4:
                        case DW_FORM_ref8:
                        case DW_FORM_ref_udata:
                            {
                                Dwarf_Off relative_offset;
                                Dwarf_Off global_offset;
                                Dwarf_Bool is_info = DWARF_FALSE_;
                                DIE_DWARF_CALL_ASSERT_OK(dwarf_formref,
                                                         attr,
                                                         &relative_offset,
                                                         &is_info);
                                DIE_DWARF_CALL_ASSERT_OK(dwarf_convert_to_global_offset,
                                                         attr,
                                                         relative_offset,
                                                         &global_offset);
                                return dwarf_->getDieFromOffset(global_offset);
                            }
                        case DW_FORM_ref_sig8: /* DWARF4 */
                            {
                                Dwarf_Sig8 sig8data;
                                DIE_DWARF_CALL_ASSERT_OK(dwarf_formsig8, attr, &sig8data);

                                return dwarf_->getDieFromSig8(sig8data);
                            }
                        default:
                            stf_throw("Unexpected DW_AT_specification form: " << form);
                    }
                }

            public:
                explicit Die(const STFDwarf* dwarf, const Dwarf_Die die) :
                    dwarf_(dwarf),
                    die_(die)
                {
                }

                Die(const Die&) = delete;

                Die(Die&& rhs) :
                    dwarf_(rhs.dwarf_),
                    die_(rhs.die_),
                    tag_(rhs.tag_)
                {
                    rhs.nullify_();
                }

                Die& operator=(const Die&) = delete;

                inline Die& operator=(Die&& rhs) {
                    dwarf_ = rhs.dwarf_;
                    die_ = rhs.die_;
                    tag_ = rhs.tag_;

                    rhs.nullify_();

                    return *this;
                }

                ~Die() {
                    if(STF_EXPECT_TRUE(dwarf_)) {
                        dwarf_->deallocDie_(die_);
                    }
                }

                inline uint16_t getTag() const {
                    if(!tag_) {
                        DIE_DWARF_CALL_ASSERT_OK(dwarf_tag, die_, &tag_);
                    }

                    return tag_;
                }

                inline bool isInlinedSubroutine() const {
                    return getTag() == DW_TAG_inlined_subroutine;
                }

                inline bool isSubprogram() const {
                    return getTag() == DW_TAG_subprogram;
                }

                inline std::optional<Die> getSpecificationDie() const {
                    std::optional<Die> die;
                    if(const auto spec_attr = getAttribute(DW_AT_specification); spec_attr) {
                        die.emplace(getReference_(*spec_attr));
                    }
                    return die;
                }

                inline std::optional<Dwarf_Attribute> getAttribute(const uint16_t attr_num) const {
                    std::optional<Dwarf_Attribute> result;
                    Dwarf_Attribute attr = 0;
                    DIE_DWARF_CALL(dwarf_attr, die_, attr_num, &attr);

                    if(dwarf_->isOk_()) {
                        result.emplace(attr);
                    }
                    else if(attr_num != DW_AT_specification) {
                        if(const auto spec_die = getSpecificationDie(); spec_die) {
                            return spec_die->getAttribute(attr_num);
                        }
                    }

                    return result;
                }

                inline bool isInlined() const {
                    const auto inline_attr = getAttribute(DW_AT_inline);

                    Dwarf_Signed inlined = DWARF_FALSE_;

                    if(inline_attr) {
                        DIE_DWARF_CALL_ASSERT_OK(dwarf_formsdata, *inline_attr, &inlined);
                    }
                    else if(const auto spec_die = getSpecificationDie(); spec_die) {
                        return spec_die->isInlined();
                    }

                    return inlined == DW_INL_inlined || inlined == DW_INL_declared_inlined;
                }

                inline std::optional<uint64_t> getLowPC() const {
                    std::optional<uint64_t> result;
                    Dwarf_Addr low_pc = 0;

                    DIE_DWARF_CALL(dwarf_lowpc, die_, &low_pc);

                    if(dwarf_->isOk_()) {
                        result.emplace(low_pc);
                    }
                    else if(const auto spec_die = getSpecificationDie(); spec_die) {
                        return spec_die->getLowPC();
                    }
                    return result;
                }

                inline uint64_t getHighPC(const uint64_t low_pc) const {
                    Dwarf_Addr high_pc = 0;
                    Dwarf_Half form = 0;

                    DIE_DWARF_CALL(dwarf_highpc_b,
                                   die_,
                                   &high_pc,
                                   &form,
                                   nullptr);

                    if(dwarf_->isOk_()) {
                        if (form != DW_FORM_CLASS_ADDRESS && !dwarf_addr_form_is_indexed(form)) {
                            high_pc += low_pc + (high_pc == 0);
                        }
                    }
                    else {
                        if(const auto spec_die = getSpecificationDie(); spec_die) {
                            return spec_die->getHighPC(low_pc);
                        }
                        high_pc = low_pc + 1;
                    }

                    return high_pc;
                }

                inline std::optional<uint64_t> getInlineOffset() const {
                    std::optional<uint64_t> result;

                    const auto abstract_origin_attr = getAttribute(DW_AT_abstract_origin);

                    if(abstract_origin_attr) {
                        Dwarf_Off die_global_offset = 0;
                        DIE_DWARF_CALL_ASSERT_OK(dwarf_global_formref,
                                                 *abstract_origin_attr,
                                                 &die_global_offset);

                        result.emplace(die_global_offset);
                    }
                    else if(const auto spec_die = getSpecificationDie(); spec_die) {
                        return spec_die->getInlineOffset();
                    }

                    return result;
                }

                inline std::optional<std::string> getLinkageName() const {
                    std::optional<std::string> name;
                    char* name_cstr;
                    if(const auto link_name_attr = getAttribute(DW_AT_linkage_name); link_name_attr) {
                        DIE_DWARF_CALL(dwarf_formstring,
                                       *link_name_attr,
                                       &name_cstr);
                        if(dwarf_->isOk_()) {
                            name.emplace(boost::core::demangle(name_cstr));
                        }
                    }
                    else if(const auto spec_die = getSpecificationDie(); spec_die) {
                        return spec_die->getLinkageName();
                    }
                    return name;
                }

                inline std::optional<std::string> getName() const {
                    std::optional<std::string> name;
                    char* name_cstr;
                    DIE_DWARF_CALL(dwarf_diename, die_, &name_cstr);
                    if(dwarf_->isOk_()) {
                        name.emplace(boost::core::demangle(name_cstr));
                    }
                    else if(name = getLinkageName(); !name) {
                        if(const auto spec_die = getSpecificationDie(); spec_die) {
                            name = spec_die->getName();
                        }
                    }

                    return name;
                }

                inline std::vector<STFAddressRange> getRanges() const {
                    std::vector<STFAddressRange> ranges;

                    const auto range_attr = getAttribute(DW_AT_ranges);

                    if(range_attr) {
                        Dwarf_Half form;
                        DIE_DWARF_CALL_ASSERT_OK(dwarf_whatform, *range_attr, &form);

                        static constexpr Dwarf_Half DWVERSION5 = 5;
                        Dwarf_Half cu_version = 2;
                        Dwarf_Half cu_offset_size = 4;
                        Dwarf_Unsigned original_off = 0;

                        DIE_DWARF_CALL_NO_ERR(dwarf_get_version_of_die,
                                              die_,
                                              &cu_version,
                                              &cu_offset_size);
                        if(form == DW_FORM_rnglistx) {
                            DIE_DWARF_CALL(dwarf_formudata, *range_attr, &original_off);
                        }
                        else {
                            DIE_DWARF_CALL(dwarf_global_formref, *range_attr, &original_off);
                        }

                        if(dwarf_->isOk_()) {
                            if(cu_version < DWVERSION5) {
                                const auto dwarf_ranges = dwarf_->getDwarfRanges_(die_, original_off);

                                if(STF_EXPECT_TRUE(dwarf_ranges)) {
                                    Dwarf_Off die_glb_offset = 0;
                                    Dwarf_Off die_off = 0;
                                    DIE_DWARF_CALL(dwarf_die_offsets,
                                                   die_,
                                                   &die_glb_offset,
                                                   &die_off);

                                    if(STF_EXPECT_TRUE(dwarf_->isOk_())) {
                                        Dwarf_Addr base_addr = 0;
                                        for(const auto& r: *dwarf_ranges) {
                                            switch(r.dwr_type) {
                                                case DW_RANGES_ENTRY:
                                                    if(STF_EXPECT_FALSE(r.dwr_addr1 == r.dwr_addr2)) {
                                                        continue;
                                                    }

                                                    ranges.emplace_back(r.dwr_addr1 + base_addr,
                                                                        r.dwr_addr2 + base_addr);

                                                    break;
                                                case DW_RANGES_ADDRESS_SELECTION:
                                                    base_addr = r.dwr_addr2;
                                                    break;
                                                case DW_RANGES_END:
                                                    break;
                                            }
                                        }
                                    }
                                }
                            }
                            else {
                                Dwarf_Unsigned global_offset_of_rle_set = 0;
                                Dwarf_Unsigned count_rnglists_entries = 0;
                                Dwarf_Rnglists_Head rnglhead = 0;

                                DIE_DWARF_CALL(dwarf_rnglists_get_rle_head,
                                               *range_attr,
                                               form,
                                               original_off,
                                               &rnglhead,
                                               &count_rnglists_entries,
                                               &global_offset_of_rle_set);

                                if(STF_EXPECT_TRUE(dwarf_->isOk_())) {
                                    for (Dwarf_Unsigned i = 0; i < count_rnglists_entries; ++i) {
                                        Dwarf_Bool no_debug_addr_available = DWARF_FALSE_;
                                        unsigned rle_code = 0;
                                        Dwarf_Addr start_addr = 0;
                                        Dwarf_Addr end_addr = 0;

                                        DIE_DWARF_CALL(dwarf_get_rnglists_entry_fields_a,
                                                       rnglhead,
                                                       i,
                                                       nullptr,
                                                       &rle_code,
                                                       nullptr,
                                                       nullptr,
                                                       &no_debug_addr_available,
                                                       &start_addr,
                                                       &end_addr);


                                        if (STF_EXPECT_FALSE(no_debug_addr_available ||
                                                             dwarf_->isNoEntry_() ||
                                                             rle_code == DW_RLE_end_of_list)) {
                                            break;
                                        }

                                        switch(rle_code) {
                                            case DW_RLE_base_address:
                                            case DW_RLE_base_addressx:
                                                continue;

                                            case DW_RLE_startx_endx:
                                            case DW_RLE_startx_length:
                                            case DW_RLE_offset_pair:
                                            case DW_RLE_start_end:
                                            case DW_RLE_start_length:
                                                break;
                                        }

                                        if(STF_EXPECT_FALSE(start_addr == end_addr)) {
                                            continue;
                                        }

                                        ranges.emplace_back(start_addr, end_addr);
                                    }

                                    dwarf_dealloc_rnglists_head(rnglhead);
                                }
                            }
                        }
                    }

                    if(!dwarf_->isOk_()) {
                        if(const auto spec_die = getSpecificationDie(); spec_die) {
                            return spec_die->getRanges();
                        }
                        ranges.clear();
                    }

                    return ranges;
                }

                inline uint64_t getOffset() const {
                    Dwarf_Off offset;
                    DIE_DWARF_CALL_ASSERT_OK(dwarf_dieoffset, die_, &offset);
                    return offset;
                }

                template<typename Callback>
                inline void iterateChildren(Callback&& callback,
                                            const Dwarf_Bool is_info,
                                            const int in_level = 0) const {
                    Dwarf_Die child = 0;

                    DIE_DWARF_CALL(dwarf_child, die_, &child);

                    if(dwarf_->isOk_()) {
                        dwarf_->makeDie_(child).iterateSiblings(callback,
                                                               is_info,
                                                               in_level + 1);
                    }
                }

                template<typename Callback>
                inline void iterateSiblings(Callback&& callback,
                                            const Dwarf_Bool is_info,
                                            const int in_level = 0) const {
                    callback(*this);
                    iterateChildren(callback, is_info, in_level);

                    const auto sib_die = dwarf_->getSibling_(die_, is_info);

                    if(sib_die) {
                        sib_die->iterateSiblings(callback, is_info, in_level);
                    }
                }
        };

    private:
        inline Die makeDie_(const Dwarf_Die die) const {
            return Die(this, die);
        }

        inline std::optional<Die> getSibling_(const Dwarf_Die die, const Dwarf_Bool is_info) const {
            std::optional<Die> result;
            Dwarf_Die sibling_die;

            DWARF_CALL(dwarf_siblingof_b,
                       dbg_,
                       die,
                       is_info,
                       &sibling_die);

            if(isOk_()) {
                result.emplace(this, sibling_die);
            }

            return result;
        }

    public:
        explicit STFDwarf(const std::string& filename) :
            dbg_(init_(filename))
        {
        }

        ~STFDwarf() {
            if(STF_EXPECT_TRUE(dbg_)) {
                dwarf_finish(dbg_);
            }
        }

        template<typename Callback>
        inline void iterateDies(Callback&& callback) {
            while(true) {
                static constexpr Dwarf_Bool is_info = DWARF_TRUE_;
                static constexpr Dwarf_Die no_die = 0;

                DWARF_CALL(dwarf_next_cu_header_d,
                           dbg_,
                           is_info,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr);

                if(isNoEntry_()) {
                    break;
                }

                /* The CU will have a single sibling, a cu_die. */
                const auto cu_die = getSibling_(no_die, is_info);
                stf_assert(cu_die, "Error reading CU siblings");

                cu_die->iterateSiblings(callback, is_info);
            }
        }

        inline Die getDieFromOffset(const uint64_t die_offset) const {
            Dwarf_Die die;
            DWARF_CALL_ASSERT_OK(dwarf_offdie_b, dbg_, die_offset, DWARF_TRUE_, &die);
            return makeDie_(die);
        }

        inline Die getDieFromSig8(Dwarf_Sig8& sig) const {
            Dwarf_Die die;
            Dwarf_Bool is_info = DWARF_FALSE_;
            DWARF_CALL_ASSERT_OK(dwarf_find_die_given_sig8,
                                 dbg_,
                                 &sig,
                                 &die,
                                 &is_info);
            return makeDie_(die);
        }
};

#undef _DWARF_STR
#undef DWARF_STR
#undef DWARF_ERR_MSG
#undef DWARF_CALL
#undef DWARF_CALL_NO_ERR
#undef DWARF_CALL_ASSERT_OK
#undef DIE_DWARF_CALL
#undef DIE_DWARF_CALL_NO_ERR
#undef DIE_DWARF_CALL_ASSERT_OK
