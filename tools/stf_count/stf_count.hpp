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
class STFCountFilter : public stf::STFFilter {
    private:
        const bool verbose_ = false;             /**< If false, outputs all counts on a single line */
        const bool short_mode_ = false;          /**< If true, only outputs instruction count */

        uint64_t record_count_ = 0;              /**< Count all records */
        uint64_t inst_count_ = 0;                /**< Count instruction anchor records */
        uint64_t inst_record_count_ = 0;         /**< Count instruction records */
        uint64_t mem_access_count_ = 0;          /**< Count memory anchor records */
        uint64_t mem_record_count_ = 0;          /**< Count memory records */
        uint64_t comment_count_ = 0;             /**< Count escape records */
        uint64_t page_table_walk_count_ = 0;     /**< Count page table walk records */
        uint64_t uop_count_ = 0;                 /**< Count all micro-op records */
        uint64_t event_count_ = 0;               /**< Count Event records */
        uint64_t kernel_count_ = 0;              /**< Count kernel instructions */

    public:
        /**
         * Constructs an STFCountFilter
         * \param inst_reader Instruction reader object
         * \param verbose If false, all output will be on a single line
         */
        explicit STFCountFilter(stf::STFInstReader& inst_reader, bool verbose = false, bool short_mode = false) :
            stf::STFFilter(inst_reader),
            verbose_(verbose),
            short_mode_(short_mode)
        {
        }

    protected:
        const std::vector<stf::STFInst>& filter(const stf::STFInst& inst) override {
            static const std::vector<stf::STFInst> empty_inst_list;

            inst_count_++;

            if(!short_mode_) {
                if(inst.isKernelCode()) {
                    kernel_count_++;
                }

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
            }

            return empty_inst_list;
        }

        void finished() const override {
            if(short_mode_) {
                std::cout << inst_count_ << std::endl;
            }
            else {
                const std::string_view sep_char = verbose_ ? "\n" : " ";
                CommaFormatter comma(std::cout);
                comma << "total_record_count " << record_count_ << sep_char;

                if (inst_count_ > 0) {
                    comma << "inst_count " << inst_count_ << sep_char;
                    comma << "inst_record_count " << inst_record_count_ << sep_char;
                } else {
                    comma << "mem access_count " << mem_access_count_ << sep_char;
                    comma << "mem_record_count " << mem_record_count_ << sep_char;
                }

                comma << "uop_count " << uop_count_ << sep_char;

                comma << "comment_count " << comment_count_ << sep_char;
                comma << "event_count " << event_count_ << sep_char;
                comma << "kernel_count " << kernel_count_ << sep_char;
                comma << "PTE_count " << page_table_walk_count_ << sep_char;

                comma << std::endl;
            }
        }
};
