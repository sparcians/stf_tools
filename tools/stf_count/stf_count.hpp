#pragma once

#include <cstdint>
#include "stf_inst_reader.hpp"
#include "stf_filter.hpp"
#include "tools_util.hpp"
#include "formatters.hpp"

/**
 * \brief RecordCount
 *
 * Record count types
 *
 */
struct RecordCount {
    uint64_t record_count = 0;              /**< Count all records */
    uint64_t inst_count = 0;                /**< Count instruction anchor records */
    uint64_t inst_record_count = 0;         /**< Count instruction records */
    uint64_t mem_access_count = 0;          /**< Count memory anchor records */
    uint64_t mem_record_count = 0;          /**< Count memory records */
    uint64_t comment_count = 0;             /**< Count escape records */
    uint64_t escape_record_count = 0;       /**< Count comments */
    uint64_t page_table_entry_count = 0;    /**< Count page table entry records */
    uint64_t page_table_desc_count = 0;     /**< Count page table desc records */
    uint64_t page_table_data_count = 0;     /**< Count page table data records */
    uint64_t uop_count = 0;                 /**< Count all micro-op records */
    uint64_t event_count = 0;               /**< Count Event records */
};

/**
 * \class STFCountFilter
 * Filter class used for counting STF records
 */
class STFCountFilter : public stf::STFFilter<STFCountFilter> {
    private:
        const bool verbose_ = false;                     /**< If false, outputs all counts on a single line */
        const bool short_mode_ = false;                  /**< If true, only outputs instruction count */
        const bool user_mode_only_ = false;              /**< If true, only counts user-mode code */
        const bool csv_output_ = false;                  /**< If true, output results in CSV format */
        const bool cumulative_csv_ = false;              /**< If true, CSV results will be cumulativet */
        const uint64_t csv_interval_ = 0;                /**< CSV dump interval. If 0, CSV will only be dumped at the end */

        mutable bool dumped_csv_header_ = false;         /**< Set to true once CSV header has been dumped */
        mutable uint64_t record_count_ = 0;              /**< Count all records */
        mutable uint64_t inst_count_ = 0;                /**< Count instruction anchor records */
        mutable uint64_t inst_record_count_ = 0;         /**< Count instruction records */
        mutable uint64_t mem_access_count_ = 0;          /**< Count memory anchor records */
        mutable uint64_t mem_record_count_ = 0;          /**< Count memory records */
        mutable uint64_t comment_count_ = 0;             /**< Count escape records */
        mutable uint64_t page_table_walk_count_ = 0;     /**< Count page table walk records */
        mutable uint64_t uop_count_ = 0;                 /**< Count all micro-op records */
        mutable uint64_t event_count_ = 0;               /**< Count Event records */
        mutable uint64_t non_user_count_ = 0;            /**< Count kernel instructions */
        mutable uint64_t fault_count_ = 0;               /**< Count faults */
        mutable uint64_t next_csv_dump_ = 0;             /**< Last time CSV was dumped */

        friend class stf::STFFilter<STFCountFilter>;

        inline void dumpCSVHeader_() const {
            if(STF_EXPECT_FALSE(!dumped_csv_header_)) {
                std::cout << "total_record_count,"
                             "inst_count,"
                             "inst_record_count,"
                             "mem access_count,"
                             "mem_record_count,"
                             "uop_count,"
                             "comment_count,"
                             "event_count,"
                             "non_user_count,"
                             "fault_count,"
                             "PTE_count" << std::endl;
                dumped_csv_header_ = true;
            }
        }

        inline void dumpCSV_() const {
            dumpCSVHeader_();

            std::cout << record_count_ << ','
                      << inst_count_ << ','
                      << inst_record_count_ << ','
                      << mem_access_count_ << ','
                      << mem_record_count_ << ','
                      << uop_count_ << ','
                      << comment_count_ << ','
                      << event_count_ << ','
                      << non_user_count_ << ','
                      << fault_count_ << ','
                      << page_table_walk_count_ << std::endl;

            if(!cumulative_csv_) {
                record_count_ = 0;
                inst_count_ = 0;
                inst_record_count_ = 0;
                mem_access_count_ = 0;
                mem_record_count_ = 0;
                uop_count_ = 0;
                comment_count_ = 0;
                event_count_ = 0;
                non_user_count_ = 0;
                fault_count_ = 0;
                page_table_walk_count_ = 0;
                next_csv_dump_ = 0;
            }
        }

    public:
        /**
         * Constructs an STFCountFilter
         * \param inst_reader Instruction reader object
         * \param verbose If false, all output will be on a single line
         */
        explicit STFCountFilter(stf::STFInstReader& inst_reader,
                                const bool verbose,
                                const bool short_mode,
                                const bool user_mode_only,
                                const bool csv_output,
                                const bool cumulative_csv,
                                const uint64_t csv_interval) :
            stf::STFFilter<STFCountFilter>(inst_reader),
            verbose_(verbose),
            short_mode_(short_mode),
            user_mode_only_(user_mode_only),
            csv_output_(csv_output),
            cumulative_csv_(cumulative_csv),
            csv_interval_(csv_interval),
            next_csv_dump_(csv_interval)
        {
        }

    protected:
        inline const std::vector<stf::STFInst>& filter(const stf::STFInst& inst) {
            const bool count_inst = !is_fault_ && (!user_mode_only_ || in_user_code_);

            if(STF_EXPECT_TRUE(count_inst)) {
                inst_count_++;
            }

            if(!short_mode_) {
                if(STF_EXPECT_FALSE(!is_fault_ && !in_user_code_)) {
                    non_user_count_++;
                }

                fault_count_ += is_fault_;

                if(STF_EXPECT_TRUE(count_inst)) {
                    const auto& orig_records = inst.getOrigRecords();
                    record_count_ += orig_records.size();
                    inst_record_count_ += orig_records.count(stf::descriptors::internal::Descriptor::STF_INST_OPCODE16);
                    inst_record_count_ += orig_records.count(stf::descriptors::internal::Descriptor::STF_INST_OPCODE32);
                    uop_count_ += orig_records.count(stf::descriptors::internal::Descriptor::STF_INST_MICROOP);
                    const size_t num_mem_accesses = orig_records.count(stf::descriptors::internal::Descriptor::STF_INST_MEM_ACCESS);
                    const size_t num_mem_content = orig_records.count(stf::descriptors::internal::Descriptor::STF_INST_MEM_CONTENT);
                    mem_access_count_ += num_mem_accesses;
                    mem_record_count_ += num_mem_accesses + num_mem_content;
                    comment_count_ += orig_records.count(stf::descriptors::internal::Descriptor::STF_COMMENT);
                    page_table_walk_count_ += orig_records.count(stf::descriptors::internal::Descriptor::STF_COMMENT);
                    event_count_ += orig_records.count(stf::descriptors::internal::Descriptor::STF_EVENT);

                    if(STF_EXPECT_FALSE(csv_output_ && (inst_count_ == next_csv_dump_))) {
                        dumpCSV_();
                        next_csv_dump_ += csv_interval_;
                    }
                }
            }

            return EMPTY_INST_LIST_;
        }

        void finished() const {
            if(short_mode_) {
                std::cout << inst_count_ << std::endl;
            }
            else if(csv_output_ && (inst_count_ != next_csv_dump_)) {
                // only dump CSV if we haven't already dumped this row
                dumpCSV_();
            }
            else {
                const std::string_view sep_char = verbose_ ? "\n" : " ";
                CommaFormatter comma(std::cout);
                comma << "total_record_count " << record_count_ << sep_char;

                if (inst_count_ > 0) {
                    comma << "inst_count " << inst_count_ << sep_char
                          << "inst_record_count " << inst_record_count_ << sep_char;
                } else {
                    comma << "mem access_count " << mem_access_count_ << sep_char
                          << "mem_record_count " << mem_record_count_ << sep_char;
                }

                comma << "uop_count " << uop_count_ << sep_char
                      << "comment_count " << comment_count_ << sep_char
                      << "event_count " << event_count_ << sep_char
                      << "non_user_count " << non_user_count_ << sep_char
                      << "fault_count " << fault_count_ << sep_char
                      << "PTE_count " << page_table_walk_count_ << sep_char
                      << std::endl;
            }
        }
};
