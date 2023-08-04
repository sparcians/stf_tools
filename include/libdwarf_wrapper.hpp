#pragma once

#include <optional>
#include <stdexcept>

#include "stf_exception.hpp"

extern "C" {
#include "dwarf.h"
#include "libdwarf.h"
}

#define _DWARF_ERR_STR(s) #s
#define DWARF_ERR_STR(s) _DWARF_ERR_STR(s)
#define DWARF_ERR_MSG(func) "Error calling libdwarf function " #func " at " __FILE__ ":" DWARF_ERR_STR(__LINE__)

#define DWARF_CALL(func, ...) dwarfCall_(DWARF_ERR_MSG(func), func, __VA_ARGS__)
#define DWARF_CALL_NO_ERR(func, ...) dwarfCallNoErr_(DWARF_ERR_MSG(func), func, __VA_ARGS__)
#define DWARF_CALL_ASSERT_OK(func, ...) dwarfCallAssertOk_(DWARF_ERR_MSG(func), func, __VA_ARGS__)

namespace dwarf_wrapper {
    static inline constexpr Dwarf_Bool DWARF_TRUE = 1;
    static inline constexpr Dwarf_Bool DWARF_FALSE = 0;

    class NoDwarfInfoException : public std::exception {
    };

    class DwarfError : public std::exception {
        private:
            const char* const msg_;

        public:
            explicit DwarfError(const char* const msg) :
                msg_(msg)
            {
            }

            const char* what() const noexcept override {
                return msg_;
            }
    };

    class DwarfInterface {
        private:
            template<size_t N, typename Func, typename ... Args>
            inline void dwarfCallNoErr_(const char (&dwarf_err_msg)[N],
                                        Func&& func,
                                        Args&&... args) const {
                res_ = func(std::forward<Args>(args)...);
                if(STF_EXPECT_FALSE(isError_())) {
                    throw DwarfError(dwarf_err_msg);
                }
            }

            template<size_t N, typename Func, typename ... Args>
            inline void dwarfCallAssertOk_(const char (&dwarf_err_msg)[N],
                                           Func&& func,
                                           Args&&... args) const {
                res_ = func(std::forward<Args>(args)..., &err_);
                if(STF_EXPECT_FALSE(!isOk_())) {
                    throw DwarfError(dwarf_err_msg);
                }
            }

            template<size_t N, typename Func, typename ... Args>
            inline void dwarfCall_(const char (&dwarf_err_msg)[N],
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

            static inline Dwarf_Debug initDwarf_(const std::string& filename) {
                Dwarf_Debug dbg = nullptr;

                char pathbuf[FILENAME_MAX];

                Dwarf_Error err = nullptr;
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

            const Dwarf_Debug dbg_ = nullptr;

            mutable Dwarf_Error err_ = nullptr;
            mutable int res_ = 0;

        public:
            explicit DwarfInterface(const std::string& filename) :
                dbg_(initDwarf_(filename))
            {
            }

            ~DwarfInterface() {
                if(STF_EXPECT_TRUE(dbg_)) {
                    dwarf_finish(dbg_);
                }
            }

            inline bool nextCuHeader(const Dwarf_Bool is_info) const {
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

                return isOk_();
            }

            inline uint16_t getTag(const Dwarf_Die die) const {
                Dwarf_Half tag;
                DWARF_CALL_ASSERT_OK(dwarf_tag, die, &tag);

                return tag;
            }

            inline std::optional<uint64_t> lowPc(const Dwarf_Die die) const {
                std::optional<uint64_t> result;
                Dwarf_Addr low_pc = 0;
                DWARF_CALL(dwarf_lowpc, die, &low_pc);
                if(isOk_()) {
                    result.emplace(low_pc);
                }
                return result;
            }

            inline std::pair<Dwarf_Addr, Dwarf_Half> highPc(const Dwarf_Die die) const {
                Dwarf_Addr high_pc = 0;
                Dwarf_Half form = DW_FORM_CLASS_UNKNOWN;

                DWARF_CALL(dwarf_highpc_b,
                           die,
                           &high_pc,
                           &form,
                           nullptr);

                return std::make_pair(high_pc, form);
            }

            inline const char* dieName(const Dwarf_Die die) const {
                char* str;
                DWARF_CALL(dwarf_diename, die, &str);
                if(isOk_()) {
                    return str;
                }

                return nullptr;
            }

            inline Dwarf_Half getDieVersion(const Dwarf_Die die) const {
                Dwarf_Half die_version = 2;
                Dwarf_Half die_offset_size = 4;

                DWARF_CALL_NO_ERR(dwarf_get_version_of_die,
                                  die,
                                  &die_version,
                                  &die_offset_size);

                if(STF_EXPECT_TRUE(isOk_())) {
                    return die_version;
                }
                return 0;
            }

            inline bool getRangeListEntry(const Dwarf_Rnglists_Head head,
                                          const Dwarf_Unsigned index,
                                          unsigned& rle_code,
                                          Dwarf_Addr& start_address,
                                          Dwarf_Addr& end_address) const {
                Dwarf_Bool no_debug_addr_available = DWARF_FALSE;

                DWARF_CALL(dwarf_get_rnglists_entry_fields_a,
                           head,
                           index,
                           nullptr,
                           &rle_code,
                           nullptr,
                           nullptr,
                           &no_debug_addr_available,
                           &start_address,
                           &end_address);

                return !(no_debug_addr_available || isNoEntry_());
            }

            inline void deallocDie(const Dwarf_Die die) const {
                dwarf_dealloc_die(die);
            }

            inline std::pair<Dwarf_Rnglists_Head, Dwarf_Unsigned> getDwarfRngLists(const Dwarf_Attribute attr,
                                                                                   const Dwarf_Half form,
                                                                                   const Dwarf_Off offset) const {
                Dwarf_Rnglists_Head head = nullptr;
                Dwarf_Unsigned size = 0;

                DWARF_CALL(dwarf_rnglists_get_rle_head,
                           attr,
                           form,
                           offset,
                           &head,
                           &size,
                           nullptr);

                if(STF_EXPECT_FALSE(!isOk_())) {
                    head = nullptr;
                }

                return std::make_pair(head, size);
            }

            inline std::pair<Dwarf_Ranges*, Dwarf_Signed> getDwarfRanges(const Dwarf_Die die,
                                                                         const Dwarf_Unsigned offset) const {
                Dwarf_Ranges *rangeset = nullptr;
                Dwarf_Signed rangecount = 0;
                DWARF_CALL(dwarf_get_ranges_b,
                           dbg_,
                           offset,
                           die,
                           nullptr,
                           &rangeset,
                           &rangecount,
                           nullptr);

                if(!isOk_()) {
                    rangeset = nullptr;
                }

                return std::make_pair(rangeset, rangecount);
            }

            inline void deallocRanges(Dwarf_Ranges* rangeset, const Dwarf_Signed rangecount) const {
                if(STF_EXPECT_TRUE(rangeset)) {
                    dwarf_dealloc_ranges(dbg_, rangeset, rangecount);
                }
            }

            inline Dwarf_Attribute getAttribute(const Dwarf_Die die, const Dwarf_Half attr_num) const {
                Dwarf_Attribute attr = nullptr;
                DWARF_CALL(dwarf_attr, die, attr_num, &attr);
                return attr;
            }

            inline Dwarf_Die siblingOf(const Dwarf_Die die, const Dwarf_Bool is_info) const {
                Dwarf_Die sibling_die = nullptr;

                DWARF_CALL(dwarf_siblingof_b,
                           dbg_,
                           die,
                           is_info,
                           &sibling_die);

                return sibling_die;
            }

            inline uint64_t dieOffset(const Dwarf_Die die) const {
                Dwarf_Off offset;
                DWARF_CALL_ASSERT_OK(dwarf_dieoffset, die, &offset);
                return offset;
            }

            inline Dwarf_Die offDie(const uint64_t die_offset) const {
                Dwarf_Die die;
                DWARF_CALL_ASSERT_OK(dwarf_offdie_b, dbg_, die_offset, DWARF_TRUE, &die);
                return die;
            }

            inline Dwarf_Die findDieFromSig8(Dwarf_Sig8&& sig) const {
                Dwarf_Die die;
                Dwarf_Bool is_info = DWARF_FALSE;
                DWARF_CALL_ASSERT_OK(dwarf_find_die_given_sig8,
                                     dbg_,
                                     &sig,
                                     &die,
                                     &is_info);
                return die;
            }

            inline Dwarf_Die child(const Dwarf_Die die) const {
                Dwarf_Die child = nullptr;
                DWARF_CALL(dwarf_child, die, &child);
                return child;
            }

            inline Dwarf_Half whatForm(const Dwarf_Attribute attr) const{
                Dwarf_Half form;
                DWARF_CALL_ASSERT_OK(dwarf_whatform, attr, &form);
                return form;
            }

            inline Dwarf_Off formRef(const Dwarf_Attribute attr) const {
                Dwarf_Off relative_offset;
                Dwarf_Bool is_info = DWARF_FALSE;
                DWARF_CALL_ASSERT_OK(dwarf_formref,
                                     attr,
                                     &relative_offset,
                                     &is_info);
                return relative_offset;
            }

            inline Dwarf_Off globalFormRef(const Dwarf_Attribute attr) const {
                Dwarf_Off offset;
                DWARF_CALL_ASSERT_OK(dwarf_global_formref, attr, &offset);
                return offset;
            }

            inline std::optional<Dwarf_Off> globalFormRefOptional(const Dwarf_Attribute attr) const {
                std::optional<Dwarf_Off> result;
                Dwarf_Off offset;
                dwarf_global_formref(attr, &offset, &err_);
                if(STF_EXPECT_TRUE(isOk_())) {
                    result.emplace(offset);
                }
                return result;
            }

            inline const char* formString(const Dwarf_Attribute attr) const {
                char* str;
                DWARF_CALL(dwarf_formstring,
                           attr,
                           &str);
                if(isOk_()) {
                    return str;
                }

                return nullptr;
            }

            inline Dwarf_Signed formSData(const Dwarf_Attribute attr) const {
                Dwarf_Signed data;
                DWARF_CALL_ASSERT_OK(dwarf_formsdata, attr, &data);
                return data;
            }

            inline Dwarf_Unsigned formUData(const Dwarf_Attribute attr) const {
                Dwarf_Unsigned val;
                DWARF_CALL_ASSERT_OK(dwarf_formudata, attr, &val);
                return val;
            }

            inline Dwarf_Off convertToGlobalOffset(const Dwarf_Attribute attr,
                                                   const Dwarf_Off relative_offset) const {
                Dwarf_Off global_offset;
                DWARF_CALL_ASSERT_OK(dwarf_convert_to_global_offset,
                                     attr,
                                     relative_offset,
                                     &global_offset);
                return global_offset;
            }

            inline Dwarf_Sig8 formSig8(const Dwarf_Attribute attr) const {
                Dwarf_Sig8 sig8data;
                DWARF_CALL_ASSERT_OK(dwarf_formsig8, attr, &sig8data);
                return sig8data;
            }
    };
} // end namespace dwarf_wrapper

#undef _DWARF_ERR_STR
#undef DWARF_ERR_STR
#undef DWARF_ERR_MSG
#undef DWARF_CALL
#undef DWARF_CALL_NO_ERR
#undef DWARF_CALL_ASSERT_OK
